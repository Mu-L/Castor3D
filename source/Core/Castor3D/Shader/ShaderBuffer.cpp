#include "Castor3D/Shader/ShaderBuffer.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Render/RenderSystem.hpp"

#include <ashespp/Buffer/Buffer.hpp>
#include <ashespp/Buffer/UniformBuffer.hpp>
#include <ashespp/Core/Device.hpp>
#include <ashespp/Descriptor/DescriptorSet.hpp>
#include <ashespp/Descriptor/DescriptorSetLayout.hpp>

using namespace castor;

namespace castor3d
{
	//*********************************************************************************************

	namespace
	{
		ashes::BufferBasePtr doCreateBuffer( Engine & engine
			, VkDeviceSize size
			, castor::String name )
		{
			ashes::BufferBasePtr result;
			auto & renderSystem = *engine.getRenderSystem();
			VkBufferUsageFlagBits target = renderSystem.getGpuInformations().hasFeature( GpuFeature::eShaderStorageBuffers )
					? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
					: VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
			auto & device = getCurrentRenderDevice( engine );
			result = makeBufferBase( device
				, size
				, target | VK_BUFFER_USAGE_TRANSFER_DST_BIT
				, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				, std::move( name ) );
			return result;
		}

		ashes::BufferViewPtr doCreateView( Engine & engine
			, VkFormat tboFormat
			, ashes::BufferBase const & buffer )
		{
			ashes::BufferViewPtr result;

			if ( !engine.getRenderSystem()->getGpuInformations().hasFeature( GpuFeature::eShaderStorageBuffers ) )
			{
				result = getCurrentRenderDevice( engine )->createBufferView( buffer
					, tboFormat
					, 0u
					, uint32_t( buffer.getSize() ) );
			}

			return result;
		}
	}

	//*********************************************************************************************

	ShaderBuffer::ShaderBuffer( Engine & engine
		, uint32_t size
		, castor::String name
		, VkFormat tboFormat )
		: m_engine{ engine }
		, m_size{ size }
		, m_buffer{ doCreateBuffer( engine, m_size, name ) }
		, m_bufferView{ doCreateView( engine, tboFormat, *m_buffer ) }
		, m_data( size_t( m_size ), uint8_t( 0 ) )
	{
	}

	ShaderBuffer::~ShaderBuffer()
	{
		m_bufferView.reset();
		m_buffer.reset();
	}

	void ShaderBuffer::update()
	{
		doUpdate( 0u, ashes::WholeSize );
	}

	void ShaderBuffer::update( VkDeviceSize offset
		, VkDeviceSize size )
	{
		auto & device = getCurrentRenderDevice( m_engine );
		doUpdate( 0u
			, std::min( m_size
				, ashes::getAlignedSize( size
					, uint32_t( device.properties.limits.nonCoherentAtomSize ) ) ) );
	}

	VkDescriptorSetLayoutBinding ShaderBuffer::createLayoutBinding( uint32_t index )const
	{
		if ( m_bufferView )
		{
			return { index, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT };
		}
		else
		{
			return { index, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT };
		}
	}

	void ShaderBuffer::createBinding( ashes::DescriptorSet & descriptorSet
		, VkDescriptorSetLayoutBinding const & binding )const
	{
		if ( m_bufferView )
		{
			descriptorSet.createBinding( binding
				, *m_buffer
				, *m_bufferView
				, 0u );
		}
		else
		{
			descriptorSet.createBinding( binding
				, *m_buffer
				, 0u
				, uint32_t( m_size ) );
		}
	}

	void ShaderBuffer::doUpdate( VkDeviceSize offset
		, VkDeviceSize size )
	{
		CU_Require( ( offset + size <= m_size )
			|| ( offset == 0u && size == ashes::WholeSize ) );

		if ( uint8_t * buffer = m_buffer->lock( offset
			, size
			, 0u ) )
		{
			std::memcpy( buffer, m_data.data(), std::min( size, m_size ) );
			m_buffer->flush( offset, size );
			m_buffer->unlock();
		}
	}
}