//
// Copyright © 2017 Arm Ltd. All rights reserved.
// See LICENSE file in the project root for full license information.
//

#include "NeonDepthwiseConvolutionUint8Workload.hpp"
#include "backends/NeonLayerSupport.hpp"
#include "backends/CpuTensorHandle.hpp"
#include "backends/ArmComputeTensorUtils.hpp"


namespace armnn
{
using namespace armcomputetensorutils;

NeonDepthwiseConvolutionUint8Workload::NeonDepthwiseConvolutionUint8Workload(
    const DepthwiseConvolution2dQueueDescriptor& descriptor,
    const WorkloadInfo& info)
    : Uint8Workload<DepthwiseConvolution2dQueueDescriptor>(descriptor, info)
{
    const TensorInfo& weightInfo = m_Data.m_Weight->GetTensorInfo();

    std::string reasonIfUnsupported;
    if (!IsNeonDepthwiseConvolution2dDescParamsSupported(&reasonIfUnsupported, m_Data.m_Parameters, weightInfo))
    {
        throw UnimplementedException(reasonIfUnsupported);
    }

    BuildArmComputeTensor(m_KernelTensor, weightInfo);

    arm_compute::Tensor* optionalBias = nullptr;
    if (m_Data.m_Parameters.m_BiasEnabled)
    {
        BuildArmComputeTensor(m_BiasTensor, m_Data.m_Bias->GetTensorInfo());
        optionalBias = &m_BiasTensor;
    }

    arm_compute::PadStrideInfo padStrideInfo(m_Data.m_Parameters.m_StrideX,
                                             m_Data.m_Parameters.m_StrideY,
                                             m_Data.m_Parameters.m_PadLeft,
                                             m_Data.m_Parameters.m_PadRight,
                                             m_Data.m_Parameters.m_PadTop,
                                             m_Data.m_Parameters.m_PadBottom,
                                             arm_compute::DimensionRoundingType::FLOOR);

    m_Data.ValidateInputsOutputs("NeonDepthwiseConvolutionUint8Workload", 1, 1);

    arm_compute::ITensor& input  = static_cast<INeonTensorHandle*>(m_Data.m_Inputs[0])->GetTensor();
    arm_compute::ITensor& output = static_cast<INeonTensorHandle*>(m_Data.m_Outputs[0])->GetTensor();

    bool use3x3Optimisation = weightInfo.GetShape()[3] == 3 && weightInfo.GetShape()[2] == 3;
    if (use3x3Optimisation)
    {
        m_pDepthwiseConvolutionLayer = std::make_unique<arm_compute::NEDepthwiseConvolutionLayer3x3>();
        static_cast<arm_compute::NEDepthwiseConvolutionLayer3x3*>(
            m_pDepthwiseConvolutionLayer.get())->configure(&input,
                                                           &m_KernelTensor,
                                                           optionalBias,
                                                           &output,
                                                           padStrideInfo);
    }
    else
    {
        m_pDepthwiseConvolutionLayer = std::make_unique<arm_compute::NEDepthwiseConvolutionLayer>();
        static_cast<arm_compute::NEDepthwiseConvolutionLayer*>(
            m_pDepthwiseConvolutionLayer.get())->configure(&input,
                                                           &m_KernelTensor,
                                                           optionalBias,
                                                           &output,
                                                           padStrideInfo);
    }

    BOOST_ASSERT(m_pDepthwiseConvolutionLayer);

    InitialiseArmComputeTensorData(m_KernelTensor, m_Data.m_Weight->GetConstTensor<uint8_t>());

    if (optionalBias)
    {
        InitialiseArmComputeTensorData(*optionalBias, m_Data.m_Bias->GetConstTensor<int32_t>());
    }
}

void NeonDepthwiseConvolutionUint8Workload::Execute() const
{
    ARMNN_SCOPED_PROFILING_EVENT(Compute::GpuAcc, "NeonDepthwiseConvolutionUint8Workload_Execute");
    BOOST_ASSERT(m_pDepthwiseConvolutionLayer);

    m_pDepthwiseConvolutionLayer->run();
}

} //namespace armnn
