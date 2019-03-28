#include "Castor3D/Technique/RenderTechniquePass.hpp"

#include "Castor3D/Mesh/Submesh.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderTarget.hpp"
#include "Castor3D/Render/RenderNode/RenderNode_Render.hpp"
#include "Castor3D/Shader/Program.hpp"
#include "Castor3D/Shader/Shaders/GlslShadow.hpp"
#include "Castor3D/Shader/Shaders/GlslMaterial.hpp"

#include <ShaderWriter/Source.hpp>

using namespace castor;

namespace castor3d
{
	namespace
	{
		template< typename MapType >
		void doSortAlpha( MapType & input
			, Camera const & camera
			, RenderPass::DistanceSortedNodeMap & output )
		{
			for ( auto & itPipelines : input )
			{
				for ( auto & renderNode : itPipelines.second )
				{
					Matrix4x4r worldMeshMatrix = renderNode.sceneNode.getDerivedTransformationMatrix().getInverse().transpose();
					Point3r worldCameraPosition = camera.getParent()->getDerivedPosition();
					Point3r meshCameraPosition = worldCameraPosition * worldMeshMatrix;
					renderNode.data.sortByDistance( meshCameraPosition );
					meshCameraPosition -= renderNode.sceneNode.getPosition();
					output.emplace( point::lengthSquared( meshCameraPosition ), makeDistanceNode( renderNode ) );
				}
			}
		}
	}

	//*************************************************************************************************

	RenderTechniquePass::RenderTechniquePass( String const & category
		, String const & name
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler
		, bool environment
		, SceneNode const * ignored
		, SsaoConfig const & config )
		: RenderPass{ category, name, *culler.getScene().getEngine(), matrixUbo, culler, ignored }
		, m_scene{ culler.getScene() }
		, m_camera{ culler.hasCamera() ? &culler.getCamera() : nullptr }
		, m_sceneNode{}
		, m_environment{ environment }
		, m_ssaoConfig{ config }
	{
	}

	RenderTechniquePass::RenderTechniquePass( String const & category
		, String const & name
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler
		, bool oit
		, bool environment
		, SceneNode const * ignored
		, SsaoConfig const & config )
		: RenderPass{ category, name, *culler.getScene().getEngine(), matrixUbo, culler, oit, ignored }
		, m_scene{ culler.getScene() }
		, m_camera{ culler.hasCamera() ? &culler.getCamera() : nullptr }
		, m_sceneNode{}
		, m_environment{ environment }
		, m_ssaoConfig{ config }
	{
	}

	RenderTechniquePass::~RenderTechniquePass()
	{
	}

	void RenderTechniquePass::accept( RenderTechniqueVisitor & visitor )
	{
	}

	void RenderTechniquePass::doFillUboDescriptor( ashes::DescriptorSetLayout const & layout
		, BillboardListRenderNode & node )
	{
	}

	void RenderTechniquePass::doFillUboDescriptor( ashes::DescriptorSetLayout const & layout
		, SubmeshRenderNode & node )
	{
	}

	void RenderTechniquePass::doFillTextureDescriptor( ashes::DescriptorSetLayout const & layout
		, uint32_t & index
		, BillboardListRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		node.passNode.fillDescriptor( layout
			, index
			, *node.texDescriptorSet );
	}

	void RenderTechniquePass::doFillTextureDescriptor( ashes::DescriptorSetLayout const & layout
		, uint32_t & index
		, SubmeshRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		ashes::WriteDescriptorSetArray writes;
		node.passNode.fillDescriptor( layout
			, index
			, writes );
		node.texDescriptorSet->setBindings( writes );
	}

	void RenderTechniquePass::doUpdate( RenderInfo & info
		, Point2r const & jitter )
	{
		doUpdateNodes( m_renderQueue.getCulledRenderNodes()
			, jitter
			, info );
		doUpdateUbos( *m_camera
			, jitter );
	}

	void RenderTechniquePass::doUpdateNodes( SceneCulledRenderNodes & nodes
		, Point2r const & jitter
		, RenderInfo & info )const
	{
		if ( nodes.hasNodes() )
		{
			RenderPass::doUpdate( nodes.instancedStaticNodes.frontCulled );
			RenderPass::doUpdate( nodes.staticNodes.frontCulled );
			RenderPass::doUpdate( nodes.skinnedNodes.frontCulled );
			RenderPass::doUpdate( nodes.instancedSkinnedNodes.frontCulled );
			RenderPass::doUpdate( nodes.morphingNodes.frontCulled );
			RenderPass::doUpdate( nodes.billboardNodes.frontCulled );

			RenderPass::doUpdate( nodes.instancedStaticNodes.backCulled, info );
			RenderPass::doUpdate( nodes.staticNodes.backCulled, info );
			RenderPass::doUpdate( nodes.skinnedNodes.backCulled, info );
			RenderPass::doUpdate( nodes.instancedSkinnedNodes.backCulled, info );
			RenderPass::doUpdate( nodes.morphingNodes.backCulled, info );
			RenderPass::doUpdate( nodes.billboardNodes.backCulled, info );
		}
	}

	bool RenderTechniquePass::doInitialise( Size const & CU_UnusedParam( size ) )
	{
		m_finished = getCurrentDevice( *this ).createSemaphore();
		return true;
	}

	void RenderTechniquePass::doCleanup()
	{
		m_renderQueue.cleanup();
		m_finished.reset();
	}

	void RenderTechniquePass::doUpdate( RenderQueueArray & queues )
	{
		queues.emplace_back( m_renderQueue );
	}

	void RenderTechniquePass::doUpdateFlags( PipelineFlags & flags )const
	{
		addFlag( flags.programFlags, ProgramFlag::eLighting );

		if ( m_environment )
		{
			addFlag( flags.programFlags, ProgramFlag::eEnvironmentMapping );
		}
	}

	ShaderPtr RenderTechniquePass::doGetGeometryShaderSource( PipelineFlags const & flags )const
	{
		return nullptr;
	}

	void RenderTechniquePass::doUpdatePipeline( RenderPipeline & pipeline )const
	{
		m_sceneUbo.update( m_scene, m_camera );
	}

	ashes::DepthStencilState RenderTechniquePass::doCreateDepthStencilState( PipelineFlags const & flags )const
	{
		return ashes::DepthStencilState{ 0u, true, m_opaque };
	}

	ashes::ColourBlendState RenderTechniquePass::doCreateBlendState( PipelineFlags const & flags )const
	{
		return RenderPass::createBlendState( flags.colourBlendMode, flags.alphaBlendMode, 1u );
	}

	ashes::DescriptorSetLayoutBindingArray RenderTechniquePass::doCreateTextureBindings( PipelineFlags const & flags )const
	{
		auto index = MinTextureIndex;
		ashes::DescriptorSetLayoutBindingArray textureBindings;

		if ( flags.texturesCount )
		{
			textureBindings.emplace_back( index
				, ashes::DescriptorType::eCombinedImageSampler
				, ashes::ShaderStageFlag::eFragment
				, flags.texturesCount );
			index += flags.texturesCount;
		}

		return textureBindings;
	}
}
