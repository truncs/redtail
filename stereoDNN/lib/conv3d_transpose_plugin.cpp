// Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
// Full license terms provided in LICENSE.md file.

#include "internal_utils.h"
#include <cassert>
#include <iomanip>
#include <sstream>
#include "conv_utils.h"

namespace redtail { namespace tensorrt
{

using namespace nvinfer1;

// -----------------------------------------------------------------
// Transposed 3D convolution plugin.
// For dimension members naming we loosely follow the notation
// from cuDNN where:
// - transposed convolution input is denoted as dy.
// - transposed convolution output is denoted as dx.
// This makes a code more consistent as a convolution input is
// a transposed convolution output and vice versa.
// -----------------------------------------------------------------
class Conv3DTransposePlugin: public IPluginV2
{
public:
    Conv3DTransposePlugin(Conv3DType conv_type, Dims kernel_dims,
                          Dims out_dims, Dims stride_dims,
                          Dims pad_start_dims, Dims pad_end_dims,
                          Weights kernel_weights, Weights bias_weights,
                          ILogger& log, std::string name):
        conv_type_(conv_type), w_dims_(kernel_dims),
        stride_dims_(stride_dims), pad_start_dims_(pad_start_dims), pad_end_dims_(pad_end_dims),
        kernel_weights_(kernel_weights), bias_weights_(bias_weights),
        log_(log), name_(name)
    {
        // REVIEW alexeyk: TRT currently does not support FP16 data tensors so we
        // use weights tensor data type for all descriptors. In case weights
        // are in FP16 we'll do the conversion on the fly. This should be changed
        // when TRT adds full support for FP16.
        // For FP16 we support only TRUE_HALF_CONFIG mode:
        // http://docs.nvidia.com/deeplearning/sdk/cudnn-developer-guide/index.html#cudnnConvolutionForward
        data_type_ = CUDNN_DATA_FLOAT;

        // Expecting kernel to be 5D tensor in KVCRS format.
        assert(w_dims_.nbDims == 5);

        // Expecting stride to be 3D tensor in DHW format.
        assert(stride_dims.nbDims == 3);

        // Expecting padding to be 3D tensors in DHW format.
        assert(pad_start_dims.nbDims == 3);
        assert(pad_end_dims.nbDims   == 3);
        // Currently only symmetric padding is supported for H,W dims.
        assert(pad_start_dims_.d[1] == pad_end_dims_.d[1]);
        assert(pad_start_dims_.d[2] == pad_end_dims_.d[2]);
        // Special case (TF-compatible) of asymmetric padding is supported for D dim.
        assert(pad_start_dims_.d[0] == pad_end_dims_.d[0] || pad_start_dims_.d[0] == pad_end_dims_.d[0] - 1);

        // Expecting output to be 4D tensor in CDHW format.
        assert(out_dims.nbDims == 4);
        x_dims_ = Dims4(out_dims.d[0], out_dims.d[1], out_dims.d[2], out_dims.d[3]);

        b_dims_ = Dims{5, {1, 1, x_dims_.d[1], 1, 1}};

        // TRT supprots FP32/FP16 weights.
        assert(kernel_weights_.type == DataType::kFLOAT || kernel_weights_.type == DataType::kHALF);
        assert(kernel_weights_.count > 0 && kernel_weights_.values != nullptr);
        // TRT supprots FP32/FP16 weights.
        assert(bias_weights_.type == DataType::kFLOAT || bias_weights_.type == DataType::kHALF);
        assert((bias_weights_.count  > 0 && bias_weights_.values != nullptr) ||
               (bias_weights_.count == 0 && bias_weights_.values == nullptr));
        // Assume same type for simplicity.
        assert(bias_weights_.type == kernel_weights_.type);

        weights_type_ = trtToCudnnDataType(kernel_weights_.type);
    }

    Conv3DTransposePlugin(Conv3DTransposePlugin&&) = delete;

    int getNbOutputs() const noexcept override
    {
        return 1;
    }

    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) noexcept override
    {
        assert(index       == 0);
        assert(nbInputDims == 1);
        assert(inputs[0].nbDims == 4);

        y_dims_ = Dims4(inputs[0].d[0], inputs[0].d[1], inputs[0].d[2], inputs[0].d[3]);

        createDescriptors();
        // Can use batch_size == 1 to set tensor descriptors initially.
        // Set input descriptor (i.e. convolution output).
        // Note: convolution output tensor is always in cuDNN format.
        ConvUtils::setConv3DTensorDescriptor(Conv3DType::kCuDnn, y_dims_, 1, weights_type_, y_desc_, log_);
        // Set conv operation descriptors.
        ConvUtils::setConv3DOperationDescriptors(conv_type_, w_dims_, stride_dims_, pad_start_dims_,
                                                 weights_type_, w_desc_, c_desc_, log_);
        // Set output descriptors (i.e. convolution input).
        // xo_desc_ is needed in bias addition operation.
        ConvUtils::setConv3DTensorDescriptor(conv_type_,         x_dims_, 1, weights_type_, x_desc_,  log_);
        ConvUtils::setConv3DTensorDescriptor(Conv3DType::kCuDnn, x_dims_, 1, weights_type_, xo_desc_, log_);
        // Verify that dimensions match.
        std::array<int, 5> out_dims;
        CHECK(cudnnGetConvolutionNdForwardOutputDim(c_desc_, x_desc_, w_desc_, out_dims.size(), out_dims.data()));
        assert(DimsUtils::areEqual(y_dims_, Dims4{out_dims[1], out_dims[2], out_dims[3], out_dims[4]}));
        // Set bias descriptor.
        ConvUtils::setConv3DBiasDescriptor(b_dims_, weights_type_, b_desc_, log_);

        return x_dims_;
    }

    void configureWithFormat(const Dims* inputDims, int nbInputs, const Dims* outputDims, int nbOutputs,
                             DataType type, PluginFormat format, int maxBatchSize) noexcept override
    {
        assert(isValid());
        assert(nbInputs  == 1);
        assert(nbOutputs == 1);
        assert(DimsUtils::areEqual(inputDims[0],  y_dims_));
        assert(DimsUtils::areEqual(outputDims[0], x_dims_));

        max_batch_size_ = maxBatchSize;
        // Update in/out descriptors and run auto-tuner to find best (fastest) algo.
        ConvUtils::setConv3DTensorDescriptor(Conv3DType::kCuDnn, y_dims_, maxBatchSize, weights_type_, y_desc_, log_);
        ConvUtils::setConv3DTensorDescriptor(conv_type_,         x_dims_, maxBatchSize, weights_type_, x_desc_, log_);
        ConvUtils::setConv3DTensorDescriptor(Conv3DType::kCuDnn, x_dims_, maxBatchSize, weights_type_, xo_desc_, log_);
        findBestAlgo();

        const size_t elt_size = getWeightsDataTypeSize();

        // Need workspace for FP32 -> FP16 conversion.
        if (isFP16())
            workspace_bytes_ += max_batch_size_ * std::max(DimsUtils::getTensorSize(x_dims_), DimsUtils::getTensorSize(y_dims_)) * elt_size;

        // Allocate memory and copy weights.
        CHECK(cudaMalloc(&kernel_weights_d_, kernel_weights_.count * elt_size));
        CHECK(cudaMemcpy(kernel_weights_d_, kernel_weights_.values,
                         kernel_weights_.count * elt_size, cudaMemcpyHostToDevice));

        if (bias_weights_.count > 0)
        {
            CHECK(cudaMalloc(&bias_weights_d_, bias_weights_.count * elt_size));
            CHECK(cudaMemcpy(bias_weights_d_, bias_weights_.values,
                             bias_weights_.count * elt_size, cudaMemcpyHostToDevice));

        }
        log_.log(ILogger::Severity::kINFO, (name_ + ": InDims  : " + DimsUtils::toString(y_dims_)).c_str());
        log_.log(ILogger::Severity::kINFO, (name_ + ": OutDims : " + DimsUtils::toString(x_dims_)).c_str());
    }

    int initialize() noexcept override
    {
        assert(isValid());
        return 0;
    }

    void terminate() noexcept override
    {
        assert(isValid());

        if (c_desc_ != nullptr)
            CHECK(cudnnDestroyConvolutionDescriptor(c_desc_));
        if (w_desc_ != nullptr)
            CHECK(cudnnDestroyFilterDescriptor(w_desc_));
        if (x_desc_ != nullptr)
            CHECK(cudnnDestroyTensorDescriptor(x_desc_));
        if (xo_desc_ != nullptr)
            CHECK(cudnnDestroyTensorDescriptor(xo_desc_));
        if (y_desc_ != nullptr)
            CHECK(cudnnDestroyTensorDescriptor(y_desc_));
        if (b_desc_ != nullptr)
            CHECK(cudnnDestroyTensorDescriptor(b_desc_));
        if (cudnn_ != nullptr)
            CHECK(cudnnDestroy(cudnn_));

        if (kernel_weights_d_ != nullptr)
            CHECK(cudaFree(kernel_weights_d_));
        if (bias_weights_d_ != nullptr)
            CHECK(cudaFree(bias_weights_d_));

        c_desc_  = nullptr;
        w_desc_  = nullptr;
        x_desc_  = nullptr;
        xo_desc_ = nullptr;
        y_desc_  = nullptr;
        b_desc_  = nullptr;
        cudnn_   = nullptr;

        kernel_weights_d_ = nullptr;
        bias_weights_d_   = nullptr;

        assert(!isValid());
    }

    size_t getWorkspaceSize(int maxBatchSize) const noexcept override
    {
        assert(isValid());
        assert(max_batch_size_ == maxBatchSize);

        return workspace_bytes_;
    }

    int enqueue(int batchSize, void const *const *inputs, void *const *outputs, void* workspace, cudaStream_t stream) noexcept override
    {
        assert(isValid());
        // REVIEW alexeyk: for now assuming batch size always equals max batch size.
        // That's pretty strict as it disables dynamic batch sizes but fine for now.
        assert(batchSize == max_batch_size_);

        cudnnStatus_t status;
        cudaError_t   cu_status = cudaSuccess;

        CHECK(status = cudnnSetStream(cudnn_, stream));

        size_t workspace_used_bytes = 0;
        // Convert to FP16 first if needed.
        auto py = preprocessInput(batchSize, inputs[0], workspace, stream, workspace_used_bytes);
        assert(py != nullptr);
        assert(workspace_used_bytes <= workspace_bytes_);

        CHECK(status = cudnnConvolutionBackwardData(cudnn_,  &Consts::kOne, w_desc_, kernel_weights_d_,
                                                    y_desc_, py, c_desc_, best_algo_,
                                                    static_cast<uint8_t*>(workspace) + workspace_used_bytes, workspace_bytes_ - workspace_used_bytes,
                                                    &Consts::kZero, x_desc_, outputs[0]));

        if (bias_weights_.count > 0)
        {
            // REVIEW alexeyk: cuDNN limitation - only second outermost dimension is supported.
            if (b_dims_.d[1] == x_dims_.d[0])
                CHECK(status = cudnnAddTensor(cudnn_, &Consts::kOne, b_desc_, bias_weights_d_, &Consts::kOne, x_desc_, outputs[0]));
            else if (b_dims_.d[2] == x_dims_.d[1])
                CHECK(cu_status = CudaKernels::addDBiasTo3DConv(bias_weights_d_, b_dims_, static_cast<float*>(outputs[0]), x_dims_, stream));
            else
                assert(false);
        }

        // Convert back to FP32 if needed.
        postprocessOutput(batchSize, outputs[0], workspace, stream);

        return (status == CUDNN_STATUS_SUCCESS && cu_status == cudaSuccess) ? 0 : -1;
    }

    size_t getSerializationSize() const noexcept override
    {
        assert(isValid());
        return 0;
    }

    void serialize(void* buffer) const noexcept override
    {
        assert(isValid());
        // REVIEW alexeyk: implement.
        assert(false);
    }

    const char* getPluginType() const noexcept override
    {
        return "Conv3dTranspose";
    }

    const char* getPluginVersion() const noexcept override
    {
        return "2";
    }

    bool supportsFormat(DataType type, PluginFormat format) const noexcept override
    {
        assert(type == DataType::kFLOAT);
        assert(format == PluginFormat::kLINEAR);
        return true;
    }

    void setPluginNamespace(const char* libNamespace) noexcept override
    {
        namespace_ = libNamespace;
    }

    const char* getPluginNamespace() const noexcept override
    {
        return namespace_.data();
    }

    IPluginV2* clone() const noexcept override
    {
        auto* plugin = new Conv3DTransposePlugin(conv_type_, w_dims_, x_dims_,
                                                 stride_dims_, pad_start_dims_, pad_end_dims_,
                                                 kernel_weights_, bias_weights_,
                                                 log_, name_);
        return plugin;
    }

    void destroy() noexcept override
    {
        delete this;
    }

private:
    bool isValid() const
    {
        return cudnn_ != nullptr;
    }

    bool isFP16() const
    {
        return weights_type_ == CUDNN_DATA_HALF;
    }

    size_t getWeightsDataTypeSize() const
    {
        return (isFP16() ? sizeof(uint16_t) : sizeof(float));
    }

    const void* preprocessInput(int batchSize, const void* y, void* workspace, cudaStream_t stream, size_t& workspace_used_bytes)
    {
        if (!isFP16())
            return y;

        assert(data_type_ == CUDNN_DATA_FLOAT);

        // Convert to FP16 using workspace.
        size_t y_size = batchSize * DimsUtils::getTensorSize(y_dims_);
        CHECK(CudaKernels::fp32Tofp16((const float*)y, (uint16_t*)workspace, y_size, stream));

        workspace_used_bytes = y_size * sizeof(uint16_t);
        return workspace;
    }

    void postprocessOutput(int batchSize, void* x, void* workspace, cudaStream_t stream)
    {
        if (!isFP16())
            return;

        assert(data_type_ == CUDNN_DATA_FLOAT);

        size_t x_size = batchSize * DimsUtils::getTensorSize(x_dims_);
        // Copy to workspace first.
        CHECK(cudaMemcpyAsync(workspace, x, x_size * sizeof(uint16_t), cudaMemcpyDeviceToDevice, stream));
        // Convert to FP32 from workspace.
        CHECK(CudaKernels::fp16Tofp32((const uint16_t*)workspace, (float*)x, x_size, stream));
    }

    void createDescriptors()
    {
        if (cudnn_ == nullptr)
            CHECK(cudnnCreate(&cudnn_));
        if (x_desc_ == nullptr)
            CHECK(cudnnCreateTensorDescriptor(&x_desc_));
        if (xo_desc_ == nullptr)
            CHECK(cudnnCreateTensorDescriptor(&xo_desc_));
        if (y_desc_ == nullptr)
            CHECK(cudnnCreateTensorDescriptor(&y_desc_));
        if (w_desc_ == nullptr)
            CHECK(cudnnCreateFilterDescriptor(&w_desc_));
        if (c_desc_ == nullptr)
            CHECK(cudnnCreateConvolutionDescriptor(&c_desc_));
        if (b_desc_ == nullptr)
            CHECK(cudnnCreateTensorDescriptor(&b_desc_));
    }

    void findBestAlgo()
    {
        // Let's hope cuDNN team will not come up with more than that number of algos (7 in cuDNN 7).
        const int algo_count = 20;
        int       res_algo_count;
        cudnnConvolutionBwdDataAlgoPerf_t algos[algo_count];
        // REVIEW alexeyk: cuDNN issue: it looks like cudnnConvolutionBackwardData hangs when
        // CUDNN_CONVOLUTION_BWD_DATA_ALGO_1 is chosen. Same problem with Ex version.
#if false
        // CHECK(cudnnFindConvolutionBackwardDataAlgorithm(cudnn_, w_desc_, y_desc_, c_desc_, x_desc_,
        //                                                 algo_count, &res_algo_count, algos));
#else
        CHECK(cudnnGetConvolutionBackwardDataAlgorithm_v7(cudnn_, w_desc_, y_desc_, c_desc_, x_desc_,
                                                          algo_count, &res_algo_count, algos));
#endif
        assert(res_algo_count > 0);
        assert(algos[0].status == CUDNN_STATUS_SUCCESS);

        // Best algo is the first.
        best_algo_       = algos[0].algo;
        workspace_bytes_ = algos[0].memory;

        // Log results.
        log_.log(ILogger::Severity::kINFO, (name_ + ": --> Conv3DTranspose layer tuning results:").c_str());
        for (auto& a: algos)
        {
            if (a.status != CUDNN_STATUS_SUCCESS)
                break;
            std::ostringstream str;
            str <<  a.algo << ": " << std::fixed << std::setw(8) << std::setprecision(1) << a.time << "ms, "
                << std::fixed << std::setw(8) << a.memory << "B";
            log_.log(ILogger::Severity::kINFO, str.str().c_str());
        }
        log_.log(ILogger::Severity::kINFO, (name_ + ": <-- Conv3DTranspose layer tuning results.").c_str());
    }

private:
    Conv3DType      conv_type_;
    cudnnDataType_t data_type_;
    cudnnDataType_t weights_type_;

    // Using Dims4 to represent 3D convos input/output is an ugly workaround
    // of TRT limitations which currently result in assert in the guts of TRT.
    Dims4 x_dims_;
    Dims4 y_dims_;
    Dims     w_dims_;
    Dims     stride_dims_;
    Dims     pad_start_dims_;
    Dims     pad_end_dims_;
    Dims     b_dims_;

    int      max_batch_size_ = 0;

    // Kernel weights on the host.
    Weights kernel_weights_;
    // Kernel weights on the device.
    float*  kernel_weights_d_ = nullptr;

    // Bias weights on the host.
    Weights bias_weights_;
    // Bias weights on the device.
    float*  bias_weights_d_ = nullptr;

    cudnnHandle_t                cudnn_   = nullptr;
    cudnnTensorDescriptor_t      x_desc_  = nullptr;
    cudnnTensorDescriptor_t      xo_desc_ = nullptr; // Original descriptor of x.
    cudnnTensorDescriptor_t      y_desc_  = nullptr;
    cudnnFilterDescriptor_t      w_desc_  = nullptr;
    cudnnConvolutionDescriptor_t c_desc_  = nullptr;
    cudnnTensorDescriptor_t      b_desc_  = nullptr;

    cudnnConvolutionBwdDataAlgo_t best_algo_       = (cudnnConvolutionBwdDataAlgo_t)-1;
    size_t                        workspace_bytes_ = 0;

    ILogger&    log_;
    std::string name_;
    std::string namespace_;
};

// Factory method.
IPluginV2* PluginContainer::createConv3DTransposePlugin(Conv3DType conv_type, Dims kernel_dims, Dims out_dims,
                                                      Dims stride_dims, Dims pad_start_dims, Dims pad_end_dims,
                                                      Weights kernel_weights, Weights bias_weights,
                                                      std::string name)
{
    std::lock_guard<std::mutex> lock(lock_);
    plugins_.push_back(new Conv3DTransposePlugin(conv_type, kernel_dims, out_dims,
                                                 stride_dims, pad_start_dims, pad_end_dims,
                                                 kernel_weights, bias_weights,
                                                 log_, name));
    return plugins_.back();
}

} }
