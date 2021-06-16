/*
** Copyright (c) 2021 LunarG, Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/

#include "encode/dx12_state_writer.h"

#include "encode/custom_dx12_struct_encoders.h"
#include "graphics/dx12_util.h"

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(encode)

Dx12StateWriter::Dx12StateWriter(util::FileOutputStream* output_stream,
                                 util::Compressor*       compressor,
                                 format::ThreadId        thread_id) :
    output_stream_(output_stream),
    compressor_(compressor), thread_id_(thread_id), encoder_(&parameter_stream_)
{
    assert(output_stream != nullptr);
    assert(compressor != nullptr);
}

Dx12StateWriter::~Dx12StateWriter() {}

void Dx12StateWriter::WriteState(const Dx12StateTable& state_table, uint64_t frame_number)
{
    format::Marker marker;
    marker.header.size  = sizeof(marker.marker_type) + sizeof(marker.frame_number);
    marker.header.type  = format::kStateMarkerBlock;
    marker.marker_type  = format::kBeginMarker;
    marker.frame_number = frame_number;
    output_stream_->Write(&marker, sizeof(marker));

    WaitForCommandQueues(state_table);

    StandardCreateWrite<IDXGIFactory_Wrapper>(state_table);
    StandardCreateWrite<IDXGISurface_Wrapper>(state_table);
    StandardCreateWrite<IDXGIFactoryMedia_Wrapper>(state_table);
    StandardCreateWrite<IDXGIDecodeSwapChain_Wrapper>(state_table);
    StandardCreateWrite<IDXGIAdapter_Wrapper>(state_table);
    StandardCreateWrite<IDXGIDevice_Wrapper>(state_table);
    StandardCreateWrite<IDXGIDisplayControl_Wrapper>(state_table);
    StandardCreateWrite<IDXGIKeyedMutex_Wrapper>(state_table);
    StandardCreateWrite<IDXGIOutput_Wrapper>(state_table);
    StandardCreateWrite<IDXGIOutputDuplication_Wrapper>(state_table);
    StandardCreateWrite<IDXGIResource_Wrapper>(state_table);

    StandardCreateWrite<ID3D12Device_Wrapper>(state_table);
    StandardCreateWrite<ID3D12CommandQueue_Wrapper>(state_table);
    StandardCreateWrite<IDXGISwapChain_Wrapper>(state_table);
    StandardCreateWrite<IDXGISwapChainMedia_Wrapper>(state_table);

    StandardCreateWrite<ID3D10Blob_Wrapper>(state_table);
    StandardCreateWrite<ID3D12CommandAllocator_Wrapper>(state_table);
    StandardCreateWrite<ID3D12CommandSignature_Wrapper>(state_table);

    StandardCreateWrite<ID3D12DeviceRemovedExtendedData_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DeviceRemovedExtendedDataSettings_Wrapper>(state_table);

    WriteFenceState(state_table);
    WriteHeapState(state_table);

    StandardCreateWrite<ID3D12LifetimeOwner_Wrapper>(state_table);
    StandardCreateWrite<ID3D12LifetimeTracker_Wrapper>(state_table);
    StandardCreateWrite<ID3D12MetaCommand_Wrapper>(state_table);
    StandardCreateWrite<ID3D12ProtectedResourceSession_Wrapper>(state_table);
    StandardCreateWrite<ID3D12QueryHeap_Wrapper>(state_table);

    WriteResourceState(state_table);
    WriteDescriptorState(state_table);

    StandardCreateWrite<ID3D12RootSignature_Wrapper>(state_table);
    StandardCreateWrite<ID3D12RootSignatureDeserializer_Wrapper>(state_table);
    StandardCreateWrite<ID3D12StateObject_Wrapper>(state_table);
    StandardCreateWrite<ID3D12StateObjectProperties_Wrapper>(state_table);
    StandardCreateWrite<ID3D12SwapChainAssistant_Wrapper>(state_table);
    StandardCreateWrite<ID3D12Tools_Wrapper>(state_table);
    StandardCreateWrite<ID3D12VersionedRootSignatureDeserializer_Wrapper>(state_table);
    StandardCreateWrite<ID3DDestructionNotifier_Wrapper>(state_table);
    StandardCreateWrite<ID3D12GraphicsCommandList_Wrapper>(state_table);
    StandardCreateWrite<ID3D12PipelineLibrary_Wrapper>(state_table);
    StandardCreateWrite<ID3D12PipelineState_Wrapper>(state_table);
    StandardCreateWrite<ID3D12Debug1_Wrapper>(state_table);
    StandardCreateWrite<ID3D12Debug2_Wrapper>(state_table);
    StandardCreateWrite<ID3D12Debug_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DebugDevice1_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DebugDevice_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DebugCommandQueue_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DebugCommandList1_Wrapper>(state_table);
    StandardCreateWrite<ID3D12DebugCommandList_Wrapper>(state_table);
    StandardCreateWrite<ID3D12SharingContract_Wrapper>(state_table);
    StandardCreateWrite<ID3D12InfoQueue_Wrapper>(state_table);

    marker.marker_type = format::kEndMarker;
    output_stream_->Write(&marker, sizeof(marker));
}

void Dx12StateWriter::WriteFunctionCall(format::ApiCallId call_id, util::MemoryOutputStream* parameter_buffer)
{
    assert(parameter_buffer != nullptr);

    bool                                 not_compressed      = true;
    format::CompressedFunctionCallHeader compressed_header   = {};
    format::FunctionCallHeader           uncompressed_header = {};
    size_t                               uncompressed_size   = parameter_buffer->GetDataSize();
    size_t                               header_size         = 0;
    const void*                          header_pointer      = nullptr;
    size_t                               data_size           = 0;
    const void*                          data_pointer        = nullptr;

    if (compressor_ != nullptr)
    {
        size_t packet_size = 0;
        size_t compressed_size =
            compressor_->Compress(uncompressed_size, parameter_buffer->GetData(), &compressed_parameter_buffer_, 0);

        if ((0 < compressed_size) && (compressed_size < uncompressed_size))
        {
            data_pointer   = reinterpret_cast<const void*>(compressed_parameter_buffer_.data());
            data_size      = compressed_size;
            header_pointer = reinterpret_cast<const void*>(&compressed_header);
            header_size    = sizeof(format::CompressedFunctionCallHeader);

            compressed_header.block_header.type = format::BlockType::kCompressedFunctionCallBlock;
            compressed_header.api_call_id       = call_id;
            compressed_header.thread_id         = thread_id_;
            compressed_header.uncompressed_size = uncompressed_size;

            packet_size += sizeof(compressed_header.api_call_id) + sizeof(compressed_header.uncompressed_size) +
                           sizeof(compressed_header.thread_id) + compressed_size;

            compressed_header.block_header.size = packet_size;
            not_compressed                      = false;
        }
    }

    if (not_compressed)
    {
        size_t packet_size = 0;
        data_pointer       = reinterpret_cast<const void*>(parameter_buffer->GetData());
        data_size          = uncompressed_size;
        header_pointer     = reinterpret_cast<const void*>(&uncompressed_header);
        header_size        = sizeof(format::FunctionCallHeader);

        uncompressed_header.block_header.type = format::BlockType::kFunctionCallBlock;
        uncompressed_header.api_call_id       = call_id;
        uncompressed_header.thread_id         = thread_id_;

        packet_size += sizeof(uncompressed_header.api_call_id) + sizeof(uncompressed_header.thread_id) + data_size;

        uncompressed_header.block_header.size = packet_size;
    }

    // Write appropriate function call block header.
    output_stream_->Write(header_pointer, header_size);

    // Write parameter data.
    output_stream_->Write(data_pointer, data_size);
}

void Dx12StateWriter::WriteMethodCall(format::ApiCallId         call_id,
                                      format::HandleId          call_object_id,
                                      util::MemoryOutputStream* parameter_buffer)
{
    assert(parameter_buffer != nullptr);

    bool                               not_compressed      = true;
    format::CompressedMethodCallHeader compressed_header   = {};
    format::MethodCallHeader           uncompressed_header = {};
    size_t                             uncompressed_size   = parameter_buffer->GetDataSize();
    size_t                             header_size         = 0;
    const void*                        header_pointer      = nullptr;
    size_t                             data_size           = 0;
    const void*                        data_pointer        = nullptr;

    if (compressor_ != nullptr)
    {
        size_t packet_size = 0;
        size_t compressed_size =
            compressor_->Compress(uncompressed_size, parameter_buffer->GetData(), &compressed_parameter_buffer_, 0);

        if ((compressed_size > 0) && (compressed_size < uncompressed_size))
        {
            data_pointer   = reinterpret_cast<const void*>(compressed_parameter_buffer_.data());
            data_size      = compressed_size;
            header_pointer = reinterpret_cast<const void*>(&compressed_header);
            header_size    = sizeof(format::CompressedMethodCallHeader);

            compressed_header.block_header.type = format::BlockType::kCompressedMethodCallBlock;
            compressed_header.api_call_id       = call_id;
            compressed_header.object_id         = call_object_id;
            compressed_header.thread_id         = thread_id_;
            compressed_header.uncompressed_size = uncompressed_size;

            packet_size += sizeof(compressed_header.api_call_id) + sizeof(compressed_header.object_id) +
                           sizeof(compressed_header.uncompressed_size) + sizeof(compressed_header.thread_id) +
                           compressed_size;

            compressed_header.block_header.size = packet_size;
            not_compressed                      = false;
        }
    }

    if (not_compressed)
    {
        size_t packet_size = 0;
        data_pointer       = reinterpret_cast<const void*>(parameter_buffer->GetData());
        data_size          = uncompressed_size;
        header_pointer     = reinterpret_cast<const void*>(&uncompressed_header);
        header_size        = sizeof(format::MethodCallHeader);

        uncompressed_header.block_header.type = format::BlockType::kMethodCallBlock;
        uncompressed_header.api_call_id       = call_id;
        uncompressed_header.object_id         = call_object_id;
        uncompressed_header.thread_id         = thread_id_;

        packet_size += sizeof(uncompressed_header.api_call_id) + sizeof(compressed_header.object_id) +
                       sizeof(uncompressed_header.thread_id) + data_size;

        uncompressed_header.block_header.size = packet_size;
    }

    // Write appropriate function call block header.
    output_stream_->Write(header_pointer, header_size);

    // Write parameter data.
    output_stream_->Write(data_pointer, data_size);
}

void Dx12StateWriter::WriteHeapState(const Dx12StateTable& state_table)
{
    std::set<util::MemoryOutputStream*> processed;
    state_table.VisitWrappers([&](const ID3D12Heap_Wrapper* wrapper) {
        assert(wrapper != nullptr);
        assert(wrapper->GetObjectInfo() != nullptr);
        assert(wrapper->GetObjectInfo()->create_parameters != nullptr);

        auto wrapper_info = wrapper->GetObjectInfo();
        if (wrapper_info->open_existing_address != nullptr)
        {
            if (!WriteCreateHeapAllocationCmd(wrapper_info->open_existing_address))
            {
                GFXRECON_LOG_ERROR("Failed to retrieve memory information for address specified to "
                                   "ID3D12Device3::OpenExistingHeapFromAddress (error = %d)",
                                   GetLastError());
            }
        }

        // TODO (GH #83): Add D3D12 trimming support, handle custom state for other heap types

        WriteMethodCall(
            wrapper_info->create_call_id, wrapper_info->create_call_object_id, wrapper_info->create_parameters.get());

        WriteAddRefAndReleaseCommands(wrapper);
    });
}

bool Dx12StateWriter::WriteCreateHeapAllocationCmd(const void* address)
{
    MEMORY_BASIC_INFORMATION info{};

    auto result = VirtualQuery(address, &info, sizeof(info));
    if (result > 0)
    {
        format::CreateHeapAllocationCommand allocation_cmd;

        allocation_cmd.meta_header.block_header.type = format::BlockType::kMetaDataBlock;
        allocation_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(allocation_cmd);
        allocation_cmd.meta_header.meta_data_id      = format::MakeMetaDataId(
            format::ApiFamilyId::ApiFamily_D3D12, format::MetaDataType::kCreateHeapAllocationCommand);
        allocation_cmd.thread_id       = thread_id_;
        allocation_cmd.allocation_id   = reinterpret_cast<uint64_t>(address);
        allocation_cmd.allocation_size = info.RegionSize;

        output_stream_->Write(&allocation_cmd, sizeof(allocation_cmd));

        return true;
    }

    return false;
}

void Dx12StateWriter::WriteDescriptorState(const Dx12StateTable& state_table)
{
    std::set<util::MemoryOutputStream*> processed;
    state_table.VisitWrappers([&](const ID3D12DescriptorHeap_Wrapper* wrapper) {
        assert(wrapper != nullptr);
        assert(wrapper->GetObjectInfo() != nullptr);
        assert(wrapper->GetObjectInfo()->create_parameters != nullptr);

        auto wrapper_info = wrapper->GetObjectInfo();

        // Write heap creation call.
        WriteMethodCall(
            wrapper_info->create_call_id, wrapper_info->create_call_object_id, wrapper_info->create_parameters.get());

        WriteAddRefAndReleaseCommands(wrapper);

        // Write GetCPUDescriptorHandleForHeapStart call.
        if (wrapper_info->cpu_start != 0)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_start;
            cpu_start.ptr = wrapper_info->cpu_start;
            EncodeStruct(&encoder_, cpu_start);
            WriteMethodCall(format::ApiCallId::ApiCall_ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart,
                            wrapper->GetCaptureId(),
                            &parameter_stream_);
            parameter_stream_.Reset();
        }

        // Write GetGPUDescriptorHandleForHeapStart call.
        if (wrapper_info->gpu_start != 0)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE gpu_start;
            gpu_start.ptr = wrapper_info->gpu_start;
            EncodeStruct(&encoder_, gpu_start);
            WriteMethodCall(format::ApiCallId::ApiCall_ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart,
                            wrapper->GetCaptureId(),
                            &parameter_stream_);
            parameter_stream_.Reset();
        }

        // Write descriptor creation calls.
        for (uint32_t i = 0; i < wrapper_info->num_descriptors; ++i)
        {
            const DxDescriptorInfo& descriptor_info = wrapper_info->descriptor_info[i];
            if (descriptor_info.create_parameters != nullptr)
            {
                WriteMethodCall(descriptor_info.create_call_id,
                                descriptor_info.create_call_object_id,
                                descriptor_info.create_parameters.get());
            }
        }
    });
}

void Dx12StateWriter::WriteAddRefAndReleaseCommands(const IUnknown_Wrapper* wrapper)
{
    // TODO (GH #83): how does shared/internal ref count factor in here?

    // Add AddRef/Release commands as needed to set object ref count for replay.
    for (unsigned long i = 1; i < wrapper->GetRefCount(); ++i)
    {
        WriteAddRefCommand(wrapper->GetCaptureId(), i + 1);
    }
    for (unsigned long i = 1; i > wrapper->GetRefCount(); --i)
    {
        WriteReleaseCommand(wrapper->GetCaptureId(), i - 1);
    }
}

void Dx12StateWriter::WriteAddRefCommand(format::HandleId handle_id, unsigned long result_ref_count)
{
    encoder_.EncodeUInt32Value(result_ref_count);
    WriteMethodCall(format::ApiCallId::ApiCall_IUnknown_AddRef, handle_id, &parameter_stream_);
    parameter_stream_.Reset();
}

void Dx12StateWriter::WriteReleaseCommand(format::HandleId handle_id, unsigned long result_ref_count)
{
    encoder_.EncodeUInt32Value(result_ref_count);
    WriteMethodCall(format::ApiCallId::ApiCall_IUnknown_Release, handle_id, &parameter_stream_);
    parameter_stream_.Reset();
}

void Dx12StateWriter::WriteResourceState(const Dx12StateTable& state_table)
{
    HRESULT result = E_FAIL;

    std::unordered_map<format::HandleId, std::vector<ResourceSnapshotInfo>> resource_snapshots;

    state_table.VisitWrappers([&](ID3D12Resource_Wrapper* resource_wrapper) {
        assert((resource_wrapper != nullptr) && (resource_wrapper->GetObjectInfo() != nullptr) &&
               (resource_wrapper->GetObjectInfo()->create_parameters != nullptr));

        auto resource_info = resource_wrapper->GetObjectInfo();
        assert(resource_info->create_call_object_id != format::kNullHandleId);

        // Write the resource creation call to capture file.
        WriteMethodCall(resource_info->create_call_id,
                        resource_info->create_call_object_id,
                        resource_info->create_parameters.get());
        WriteAddRefAndReleaseCommands(resource_wrapper);

        // Collect list of subresources that need to be written to the capture file.
        for (UINT i = 0; i < resource_info->num_subresources; ++i)
        {
            ResourceSnapshotInfo snapshot_info;
            snapshot_info.resource_wrapper = resource_wrapper;
            resource_snapshots[resource_info->device_id].push_back(snapshot_info);
        }
    });

    // Write resource snapshots to the capture file.
    WriteResourceSnapshots(resource_snapshots);
}

HRESULT
Dx12StateWriter::WriteResourceSnapshots(
    const std::unordered_map<format::HandleId, std::vector<ResourceSnapshotInfo>>& snapshots)
{
    HRESULT result = S_OK;

    for (auto kvp : snapshots)
    {
        auto device_id = kvp.first;
        auto snapshots = kvp.second;

        // Write the block indicating resource processing start.
        format::BeginResourceInitCommand begin_cmd;
        begin_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(begin_cmd);
        begin_cmd.meta_header.block_header.type = format::kMetaDataBlock;
        begin_cmd.meta_header.meta_data_id      = format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_D3D12,
                                                                    format::MetaDataType::kBeginResourceInitCommand);
        begin_cmd.thread_id                     = thread_id_;
        begin_cmd.device_id                     = device_id;
        begin_cmd.max_resource_size             = 0; // TODO: use these?
        begin_cmd.max_copy_size                 = 0;

        output_stream_->Write(&begin_cmd, sizeof(begin_cmd));

        // Write each resource snapshot to the capture file.
        for (auto snapshot : kvp.second)
        {
            WriteResourceSnapshot(snapshot);
        }

        // Write the block indicating resource processing end.
        format::EndResourceInitCommand end_cmd;
        end_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(end_cmd);
        end_cmd.meta_header.block_header.type = format::kMetaDataBlock;
        end_cmd.meta_header.meta_data_id =
            format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_D3D12, format::MetaDataType::kEndResourceInitCommand);
        end_cmd.thread_id = thread_id_;
        end_cmd.device_id = device_id;

        output_stream_->Write(&end_cmd, sizeof(end_cmd));
    }

    return result;
}

HRESULT Dx12StateWriter::WriteResourceSnapshot(const ResourceSnapshotInfo& snapshot)
{
    HRESULT result = S_OK;

    ID3D12Resource_Wrapper* resource_wrapper = snapshot.resource_wrapper;
    ID3D12ResourceInfo*     resource_info    = resource_wrapper->GetObjectInfo();
    ID3D12Resource*         resource         = resource_wrapper->GetWrappedObjectAs<ID3D12Resource>();

    // TODO (GH #83): Create a mappable copy of resources that are not CPU-visible.
    ID3D12Resource* mappable_resource = resource;

    for (uint32_t i = 0; i < resource_info->num_subresources; ++i)
    {
        GFXRECON_CHECK_CONVERSION_DATA_LOSS(size_t, resource_info->subresource_sizes[i]);

        uint8_t* mapped_data      = nullptr;
        size_t   subresource_size = static_cast<size_t>(resource_info->subresource_sizes[i]);

        // Map the subresource.
        result = graphics::dx12::MapSubresource(mappable_resource, i, subresource_size, mapped_data);
        if (!SUCCEEDED(result) || (mapped_data == nullptr))
        {
            GFXRECON_LOG_ERROR("Failed to map subresource %" PRIu32 " for resource (id = %" PRIu64
                               "). Resource's data may be invalid in capture file.",
                               i,
                               resource_wrapper->GetCaptureId());
            continue;
        }

        // Create subresource upload data block.
        format::InitSubresourceCommandHeader upload_cmd;
        upload_cmd.meta_header.block_header.type = format::kMetaDataBlock;
        upload_cmd.meta_header.meta_data_id =
            format::MakeMetaDataId(format::ApiFamilyId::ApiFamily_D3D12, format::MetaDataType::kInitSubresourceCommand);
        upload_cmd.thread_id   = thread_id_;
        upload_cmd.device_id   = resource_info->device_id;
        upload_cmd.resource_id = resource_wrapper->GetCaptureId();
        upload_cmd.subresource = i;
        upload_cmd.data_size   = subresource_size;

        // Compress block data.
        if (compressor_ != nullptr)
        {
            size_t compressed_size =
                compressor_->Compress(subresource_size, mapped_data, &compressed_parameter_buffer_, 0);

            if ((compressed_size > 0) && (compressed_size < subresource_size))
            {
                upload_cmd.meta_header.block_header.type = format::BlockType::kCompressedMetaDataBlock;

                mapped_data      = compressed_parameter_buffer_.data();
                subresource_size = compressed_size;
            }
        }

        // Calculate size of packet with compressed or uncompressed data size.
        upload_cmd.meta_header.block_header.size = format::GetMetaDataBlockBaseSize(upload_cmd) + subresource_size;

        // Write upload block to file.
        output_stream_->Write(&upload_cmd, sizeof(upload_cmd));
        output_stream_->Write(mapped_data, subresource_size);

        // Unmap the subresource.
        D3D12_RANGE empty_range = { 0, 0 };
        mappable_resource->Unmap(i, &empty_range);
    }

    return result;
}

void Dx12StateWriter::WaitForCommandQueues(const Dx12StateTable& state_table)
{
    std::vector<HANDLE>       idle_events;
    std::vector<ID3D12Fence*> fences;

    const UINT64 kSignalValue = 1;

    state_table.VisitWrappers([&](ID3D12CommandQueue_Wrapper* queue_wrapper) {
        assert((queue_wrapper != nullptr) && (queue_wrapper->GetWrappedObject() != nullptr) &&
               (queue_wrapper->GetObjectInfo() != nullptr));

        auto queue_info = queue_wrapper->GetObjectInfo();
        auto queue      = queue_wrapper->GetWrappedObjectAs<ID3D12CommandQueue>();

        assert((queue_info->device_wrapper) && (queue_info->device_wrapper->GetWrappedObject() != nullptr));

        auto device_wrapper = queue_info->device_wrapper;
        auto device         = queue_info->device_wrapper->GetWrappedObjectAs<ID3D12Device>();

        // Create a fence and event, then signal it on the queue.
        ID3D12Fence* fence        = nullptr;
        auto         fence_result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (SUCCEEDED(fence_result) && (fence != nullptr))
        {
            HANDLE idle_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            fence->SetEventOnCompletion(kSignalValue, idle_event);

            queue->Signal(fence, kSignalValue);

            idle_events.push_back(idle_event);
            fences.push_back(fence);
        }
        else
        {
            GFXRECON_LOG_ERROR(
                "Failed to create ID3D12Fence object to sync ID3D12CommandQueue for trim state writing.");
        }
    });

    // Wait for the queues to signal the fence.
    for (auto idle_event : idle_events)
    {
        WaitForSingleObject(idle_event, INFINITE);
        CloseHandle(idle_event);
    }

    // Release fences.
    for (auto fence : fences)
    {
        fence->Release();
    }
}

void Dx12StateWriter::WriteFenceState(const Dx12StateTable& state_table)
{
    state_table.VisitWrappers([&](ID3D12Fence_Wrapper* fence_wrapper) {
        assert(fence_wrapper != nullptr);
        assert(fence_wrapper->GetWrappedObject() != nullptr);
        assert(fence_wrapper->GetObjectInfo() != nullptr);

        auto fence      = fence_wrapper->GetWrappedObjectAs<ID3D12Fence>();
        auto fence_info = fence_wrapper->GetObjectInfo();

        assert(fence_info->create_parameters != nullptr);
        assert(fence_info->create_call_object_id != format::kNullHandleId);

        // Write call to create the fence.
        WriteMethodCall(
            fence_info->create_call_id, fence_info->create_call_object_id, fence_info->create_parameters.get());
        WriteAddRefAndReleaseCommands(fence_wrapper);

        UINT64 completed_fence_value = fence->GetCompletedValue();

        // Write SetEventOnCompletion commands for remaining pending events.
        // The pending_events_mutex doesn't need to be locked here because all other threads are blocked while state is
        // being written.
        auto& pending_events = fence_info->pending_events;
        for (auto events : pending_events)
        {
            UINT64 value = events.first;

            // Ignore any events that have already been signaled.
            if (value <= completed_fence_value)
            {
                continue;
            }

            for (auto event : events.second)
            {
                encoder_.EncodeUInt64Value(value);
                encoder_.EncodeVoidPtr(event);
                encoder_.EncodeInt32Value(S_OK);
                WriteMethodCall(format::ApiCallId::ApiCall_ID3D12Fence_SetEventOnCompletion,
                                fence_wrapper->GetCaptureId(),
                                &parameter_stream_);
                parameter_stream_.Reset();
            }
        }

        // Write call to signal the fence to its most recent value.
        encoder_.EncodeUInt64Value(completed_fence_value);
        encoder_.EncodeInt32Value(S_OK);
        WriteMethodCall(
            format::ApiCallId::ApiCall_ID3D12Fence_Signal, fence_wrapper->GetCaptureId(), &parameter_stream_);
        parameter_stream_.Reset();
    });
}

GFXRECON_END_NAMESPACE(encode)
GFXRECON_END_NAMESPACE(gfxrecon)