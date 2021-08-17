// Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
// Full license terms provided in LICENSE.md file.

#include "internal_utils.h"
#include <cassert>

namespace redtail { namespace tensorrt
{

using namespace nvinfer1;

// -----------------------------------------------------------------
// Tensor padding plugin.
// -----------------------------------------------------------------
class PaddingPlugin: public IPluginV2
{
public:
    PaddingPlugin(Dims4 pad_start, Dims4 pad_end, ILogger& log, std::string name):
        pad_start_(pad_start), pad_end_(pad_end), log_(log), name_(name)
    {
        // Only D end padding is currently supported.
        assert(pad_start_.n() == 0);
        assert(pad_start_.c() == 0);
        assert(pad_start_.h() == 0);
        assert(pad_start_.w() == 0);

        assert(pad_end_.n() >= 0);
        assert(pad_end_.c() == 0);
        assert(pad_end_.h() == 0);
        assert(pad_end_.w() == 0);
    }

    PaddingPlugin(PaddingPlugin&&) = delete;

    int getNbOutputs() const noexcept override
    {
        return 1;
    }

    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) noexcept override
    {
        assert(nbInputDims == 1);
        // Only NCHW format is supported for now, TRT does not work (assert) with generic Dims.
        assert(inputs[0].nbDims == 4);

        in_dims_  = Dims4(inputs[0].d[0], inputs[0].d[1], inputs[0].d[2], inputs[0].d[3]);
        out_dims_ = Dims4(in_dims_.d[0] + pad_start_.d[0] + pad_end_.d[0],
                             in_dims_.d[1] + pad_start_.d[1] + pad_end_.d[1],
                             in_dims_.d[2] + pad_start_.d[2] + pad_end_.d[2],
                             in_dims_.d[3] + pad_start_.d[3] + pad_end_.d[3]);
        return out_dims_;
    }

    void configureWithFormat(const Dims* inputDims, int nbInputs, const Dims* outputDims, int nbOutputs, 
                             DataType type, PluginFormat format, int maxBatchSize) noexcept override
    {
        assert(nbInputs  == 1);
        assert(nbOutputs == 1);
        assert(DimsUtils::areEqual(inputDims[0],  in_dims_));
        assert(DimsUtils::areEqual(outputDims[0], out_dims_));

        log_.log(ILogger::Severity::kINFO, (name_ + ": InDims : " + DimsUtils::toString(in_dims_)).c_str());
        log_.log(ILogger::Severity::kINFO, (name_ + ": OutDims: " + DimsUtils::toString(out_dims_)).c_str());
    }

    int initialize() noexcept override
    {
        return 0;
    }

    void terminate() noexcept override
    {
    }

    size_t getWorkspaceSize(int maxBatchSize) const noexcept override
    {
        return 0;
    }

    int enqueue(int batchSize, void const *const *inputs, void *const *outputs, void* workspace, cudaStream_t stream) noexcept override
    {
        // Copy original tensor.
        size_t in_size_bytes = DimsUtils::getTensorSize(in_dims_) * sizeof(float);
        CHECK(cudaMemcpyAsync(outputs[0], inputs[0], in_size_bytes, cudaMemcpyDeviceToDevice, stream));

        // Zero out end D padding if needed.
        size_t out_size_bytes = DimsUtils::getTensorSize(out_dims_) * sizeof(float);
        if (out_size_bytes > in_size_bytes)
        {
            auto pdst = static_cast<unsigned char*>(outputs[0]) + in_size_bytes;
            CHECK(cudaMemsetAsync(pdst, 0, out_size_bytes - in_size_bytes, stream));
        }

        return 0;
    }

    size_t getSerializationSize() const noexcept override
    {
        return 0;
    }

    void serialize(void* buffer) const noexcept override
    {
    }

    const char* getPluginType() const noexcept override
    {
        return "Padding";
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
        auto* plugin = new PaddingPlugin(pad_start_, pad_end_, log_, name_);
        return plugin;
    }

    void destroy() noexcept override
    {
        delete this;
    }


private:

private:
    Dims4  pad_start_;
    Dims4  pad_end_;
    Dims4  in_dims_;
    Dims4  out_dims_;

    ILogger&    log_;
    std::string name_;
    std::string namespace_;
};

// Factory method.
IPluginV2* PluginContainer::createPaddingPlugin(Dims4 pad_start, Dims4 pad_end, std::string name)
{
    std::lock_guard<std::mutex> lock(lock_);
    plugins_.push_back(new PaddingPlugin(pad_start, pad_end, log_, name));
    return plugins_.back();
}

} }
