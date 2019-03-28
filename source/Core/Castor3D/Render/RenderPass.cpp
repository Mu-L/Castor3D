#include "Castor3D/Render/RenderPass.hpp"

#include "Castor3D/Castor3DPrerequisites.hpp"
#include "Castor3D/Engine.hpp"
#include "Castor3D/Material/Material.hpp"
#include "Castor3D/Material/Pass.hpp"
#include "Castor3D/Mesh/Submesh.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderNode/RenderNode_Render.hpp"
#include "Castor3D/Scene/BillboardList.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Geometry.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/SceneNode.hpp"
#include "Castor3D/Scene/Animation/AnimatedSkeleton.hpp"
#include "Castor3D/Shader/PassBuffer/PassBuffer.hpp"
#include "Castor3D/Shader/TextureConfigurationBuffer/TextureConfigurationBuffer.hpp"
#include "Castor3D/Shader/Program.hpp"
#include "Castor3D/Shader/Shaders/GlslMaterial.hpp"

#include <Ashes/Buffer/Buffer.hpp>
#include <Ashes/Buffer/VertexBuffer.hpp>

#include <ShaderWriter/Source.hpp>

using namespace castor;

#define C3D_InvertFrontAndBackPipelines 0

namespace castor3d
{
	//*********************************************************************************************

	namespace
	{
		template< typename MapType, typename FuncType >
		inline void doTraverseNodes( RenderPass const & pass
			, MapType & nodes
			, FuncType function )
		{
			for ( auto & itPipelines : nodes )
			{
				for ( auto & itPass : itPipelines.second )
				{
					for ( auto & itSubmeshes : itPass.second )
					{
						function( *itPipelines.first
							, *itPass.first
							, *itSubmeshes.first
							, itSubmeshes.first->getInstantiation()
							, itSubmeshes.second );
					}
				}
			}
		}

		template< typename MapType >
		inline void doRenderNonInstanced( RenderPass const & pass
			, MapType & nodes )
		{
			for ( auto & itPipelines : nodes )
			{
				pass.updatePipeline( *itPipelines.first );
			}
		}

		template< typename MapType >
		inline void doRenderNonInstanced( RenderPass const & pass
			, MapType & nodes
			, RenderInfo & info )
		{
			for ( auto & itPipelines : nodes )
			{
				pass.updatePipeline( *itPipelines.first );

				for ( auto & renderNode : itPipelines.second )
				{
					info.m_visibleFaceCount += details::getPrimitiveCount( renderNode->data );
					info.m_visibleVertexCount += details::getVertexCount( renderNode->data );
					++info.m_drawCalls;
					++info.m_visibleObjectsCount;
				}
			}
		}

		template< typename NodeT >
		uint32_t copyNodesMatrices( std::vector< NodeT > const & renderNodes
			, std::vector< InstantiationData > & matrixBuffer )
		{
			auto const mtxSize = sizeof( float ) * 16;
			auto const stride = sizeof( InstantiationData );
			auto const count = std::min( uint32_t( matrixBuffer.size() )
				, uint32_t( renderNodes.size() ) );
			auto buffer = matrixBuffer.data();
			auto it = renderNodes.begin();
			auto i = 0u;

			while ( i < count )
			{
				auto & node = *it;
				buffer->m_matrix = node.sceneNode.getDerivedTransformationMatrix();
				buffer->m_material = node.passNode.pass.getId();
				++buffer;
				++i;
				++it;
			}

			return count;
		}

		template< typename NodeT >
		uint32_t copyNodesMatrices( std::vector< NodeT * > const & renderNodes
			, std::vector< InstantiationData > & matrixBuffer )
		{
			auto const count = std::min( uint32_t( matrixBuffer.size() )
				, uint32_t( renderNodes.size() ) );
			auto buffer = matrixBuffer.data();
			auto it = renderNodes.begin();
			auto i = 0u;

			while ( i < count )
			{
				auto & node = *it;
				buffer->m_matrix = node->sceneNode.getDerivedTransformationMatrix();
				buffer->m_material = node->passNode.pass.getId();
				++buffer;
				++i;
				++it;
			}

			return count;
		}
	}

	//*********************************************************************************************

	RenderPass::RenderPass( String const & category
		, String const & name
		, Engine & engine
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler )
		: OwnedBy< Engine >{ engine }
		, Named{ name }
		, m_renderSystem{ *engine.getRenderSystem() }
		, m_matrixUbo{ matrixUbo }
		, m_culler{ culler }
		, m_category{ category }
		, m_oit{ true }
		, m_renderQueue{ *this, true, nullptr }
		, m_opaque{ true }
		, m_sceneUbo{ engine }
	{
	}

	RenderPass::RenderPass( String const & category
		, String const & name
		, Engine & engine
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler
		, bool oit )
		: OwnedBy< Engine >{ engine }
		, Named{ name }
		, m_renderSystem{ *engine.getRenderSystem() }
		, m_matrixUbo{ matrixUbo }
		, m_culler{ culler }
		, m_category{ category }
		, m_oit{ oit }
		, m_renderQueue{ *this, false, nullptr }
		, m_opaque{ false }
		, m_sceneUbo{ engine }
	{
	}

	RenderPass::RenderPass( String const & category
		, String const & name
		, Engine & engine
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler
		, SceneNode const * ignored )
		: OwnedBy< Engine >{ engine }
		, Named{ name }
		, m_renderSystem{ *engine.getRenderSystem() }
		, m_matrixUbo{ matrixUbo }
		, m_culler{ culler }
		, m_category{ category }
		, m_oit{ true }
		, m_renderQueue{ *this, true, ignored }
		, m_opaque{ true }
		, m_sceneUbo{ engine }
	{
	}

	RenderPass::RenderPass( String const & category
		, String const & name
		, Engine & engine
		, MatrixUbo const & matrixUbo
		, SceneCuller & culler
		, bool oit
		, SceneNode const * ignored )
		: OwnedBy< Engine >{ engine }
		, Named{ name }
		, m_renderSystem{ *engine.getRenderSystem() }
		, m_matrixUbo{ matrixUbo }
		, m_culler{ culler }
		, m_category{ category }
		, m_oit{ oit }
		, m_renderQueue{ *this, false, ignored }
		, m_opaque{ false }
		, m_sceneUbo{ engine }
	{
	}

	RenderPass::~RenderPass()
	{
	}

	bool RenderPass::initialise( Size const & size )
	{
		m_timer = std::make_shared< RenderPassTimer >( *getEngine(), m_category, getName() );
		m_sceneUbo.initialise();
		m_sceneUbo.setWindowSize( size );
		m_size = size;
		return doInitialise( size );
	}

	void RenderPass::cleanup()
	{
		m_sceneUbo.cleanup();
		m_renderPass.reset();
		doCleanup();
		m_timer.reset();
		m_backPipelines.clear();
		m_frontPipelines.clear();
	}

	void RenderPass::update( RenderQueueArray & queues )
	{
		doUpdate( queues );
	}

	ShaderPtr RenderPass::getVertexShaderSource( PipelineFlags const & flags )const
	{
		return doGetVertexShaderSource( flags );
	}

	ShaderPtr RenderPass::getGeometryShaderSource( PipelineFlags const & flags )const
	{
		return doGetGeometryShaderSource( flags );
	}

	ShaderPtr RenderPass::getPixelShaderSource( PipelineFlags const & flags )const
	{
		ShaderPtr result;

		if ( checkFlag( flags.passFlags, PassFlag::eMetallicRoughness ) )
		{
			result = doGetPbrMRPixelShaderSource( flags );
		}
		else if ( checkFlag( flags.passFlags, PassFlag::eSpecularGlossiness ) )
		{
			result = doGetPbrSGPixelShaderSource( flags );
		}
		else
		{
			result = doGetLegacyPixelShaderSource( flags );
		}

		return result;
	}

	PipelineFlags RenderPass::prepareBackPipeline( BlendMode colourBlendMode
		, BlendMode alphaBlendMode
		, ashes::CompareOp alphaFunc
		, PassFlags const & passFlags
		, TextureFlags const & textures
		, uint32_t texturesCount
		, uint32_t heightMapIndex
		, ProgramFlags const & programFlags
		, SceneFlags const & sceneFlags
		, ashes::PrimitiveTopology topology
		, ashes::VertexLayoutCRefArray const & layouts )
	{
		auto flags = PipelineFlags{ colourBlendMode
			, alphaBlendMode
			, passFlags
			, textures
			, texturesCount
			, heightMapIndex
			, programFlags
			, sceneFlags
			, topology
			, alphaFunc };
		doUpdateFlags( flags );
		remFlag( flags.programFlags, ProgramFlag::eInvertNormals );

		if ( checkFlag( passFlags, PassFlag::eAlphaBlending ) != m_opaque
			&& ( !checkFlag( programFlags, ProgramFlag::eBillboards )
				|| !isShadowMapProgram( programFlags ) ) )
		{
			if ( m_opaque )
			{
				flags.alphaBlendMode = BlendMode::eNoBlend;
			}

			auto program = doGetProgram( flags );
			doPrepareBackPipeline( program, layouts, flags );
		}

		return flags;
	}

	PipelineFlags RenderPass::prepareFrontPipeline( BlendMode colourBlendMode
		, BlendMode alphaBlendMode
		, ashes::CompareOp alphaFunc
		, PassFlags const & passFlags
		, TextureFlags const & textures
		, uint32_t texturesCount
		, uint32_t heightMapIndex
		, ProgramFlags const & programFlags
		, SceneFlags const & sceneFlags
		, ashes::PrimitiveTopology topology
		, ashes::VertexLayoutCRefArray const & layouts )
	{
		auto flags = PipelineFlags{ colourBlendMode
			, alphaBlendMode
			, passFlags
			, textures
			, texturesCount
			, heightMapIndex
			, programFlags
			, sceneFlags
			, topology
			, alphaFunc };
		doUpdateFlags( flags );
		addFlag( flags.programFlags, ProgramFlag::eInvertNormals );

		if ( checkFlag( passFlags, PassFlag::eAlphaBlending ) != m_opaque
			&& ( !checkFlag( programFlags, ProgramFlag::eBillboards )
				|| !isShadowMapProgram( programFlags ) ) )
		{
			if ( m_opaque )
			{
				flags.alphaBlendMode = BlendMode::eNoBlend;
			}

			auto program = doGetProgram( flags );
			doPrepareFrontPipeline( program, layouts, flags );
		}

		return flags;
	}

	RenderPipeline * RenderPass::getPipelineFront( PipelineFlags flags )const
	{
		if ( m_opaque )
		{
			flags.alphaBlendMode = BlendMode::eNoBlend;
		}

		auto & pipelines = doGetFrontPipelines();
		auto it = pipelines.find( flags );
		RenderPipeline * result{ nullptr };

		if ( it != pipelines.end() )
		{
			result = it->second.get();
		}

		return result;
	}

	RenderPipeline * RenderPass::getPipelineBack( PipelineFlags flags )const
	{
		if ( m_opaque )
		{
			flags.alphaBlendMode = BlendMode::eNoBlend;
		}

		auto & pipelines = doGetBackPipelines();
		auto it = pipelines.find( flags );
		RenderPipeline * result{ nullptr };

		if ( it != pipelines.end() )
		{
			result = it->second.get();
		}

		return result;
	}

	SkinningRenderNode RenderPass::createSkinningNode( Pass & pass
		, RenderPipeline & pipeline
		, Submesh & submesh
		, Geometry & primitive
		, AnimatedSkeleton & skeleton )
	{
		auto & buffers = submesh.getGeometryBuffers( pass.getOwner()->shared_from_this() );
		auto & scene = *primitive.getScene();
		auto geometryEntry = scene.getGeometryCache().getUbos( primitive, submesh, pass );
		auto animationEntry = scene.getAnimatedObjectGroupCache().getUbos( skeleton );

		return SkinningRenderNode
		{
			pipeline,
			doCreatePassRenderNode( pass, pipeline ),
			geometryEntry.modelMatrixUbo,
			geometryEntry.modelUbo,
			geometryEntry.pickingUbo,
			geometryEntry.texturesUbo,
			buffers,
			*primitive.getParent(),
			submesh,
			primitive,
			skeleton,
			animationEntry.skinningUbo
		};
	}

	MorphingRenderNode RenderPass::createMorphingNode( Pass & pass
		, RenderPipeline & pipeline
		, Submesh & submesh
		, Geometry & primitive
		, AnimatedMesh & mesh )
	{
		auto & buffers = submesh.getGeometryBuffers( pass.getOwner()->shared_from_this() );
		auto & scene = *primitive.getScene();
		auto geometryEntry = scene.getGeometryCache().getUbos( primitive, submesh, pass );
		auto animationEntry = scene.getAnimatedObjectGroupCache().getUbos( mesh );

		return MorphingRenderNode
		{
			pipeline,
			doCreatePassRenderNode( pass, pipeline ),
			geometryEntry.modelMatrixUbo,
			geometryEntry.modelUbo,
			geometryEntry.pickingUbo,
			geometryEntry.texturesUbo,
			buffers,
			*primitive.getParent(),
			submesh,
			primitive,
			mesh,
			animationEntry.morphingUbo
		};
	}

	StaticRenderNode RenderPass::createStaticNode( Pass & pass
		, RenderPipeline & pipeline
		, Submesh & submesh
		, Geometry & primitive )
	{
		auto & buffers = submesh.getGeometryBuffers( pass.getOwner()->shared_from_this() );
		auto & scene = *primitive.getScene();
		auto entry = scene.getGeometryCache().getUbos( primitive, submesh, pass );

		return StaticRenderNode
		{
			pipeline,
			doCreatePassRenderNode( pass, pipeline ),
			entry.modelMatrixUbo,
			entry.modelUbo,
			entry.pickingUbo,
			entry.texturesUbo,
			buffers,
			*primitive.getParent(),
			submesh,
			primitive,
		};
	}

	BillboardRenderNode RenderPass::createBillboardNode( Pass & pass
		, RenderPipeline & pipeline
		, BillboardBase & billboard )
	{
		auto & buffers = billboard.getGeometryBuffers();
		auto & scene = billboard.getParentScene();
		auto entry = scene.getBillboardPools().getUbos( billboard, pass );

		return BillboardRenderNode
		{
			pipeline,
			doCreatePassRenderNode( pass, pipeline ),
			entry.modelMatrixUbo,
			entry.modelUbo,
			entry.pickingUbo,
			entry.billboardUbo,
			entry.texturesUbo,
			buffers,
			*billboard.getNode(),
			billboard
		};
	}

	void RenderPass::updatePipeline( RenderPipeline & pipeline )const
	{
		doUpdatePipeline( pipeline );
	}

	void RenderPass::updateFlags( PipelineFlags & flags )const
	{
		doUpdateFlags( flags );
	}

	ashes::ColourBlendState RenderPass::createBlendState( BlendMode colourBlendMode
		, BlendMode alphaBlendMode
		, uint32_t attachesCount )
	{
		ashes::ColourBlendStateAttachment attach;

		switch ( colourBlendMode )
		{
		case BlendMode::eNoBlend:
			attach.srcColorBlendFactor = ashes::BlendFactor::eOne;
			attach.dstColorBlendFactor = ashes::BlendFactor::eZero;
			break;

		case BlendMode::eAdditive:
			attach.blendEnable = true;
			attach.srcColorBlendFactor = ashes::BlendFactor::eOne;
			attach.dstColorBlendFactor = ashes::BlendFactor::eOne;
			break;

		case BlendMode::eMultiplicative:
			attach.blendEnable = true;
			attach.srcColorBlendFactor = ashes::BlendFactor::eZero;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcColour;
			break;

		case BlendMode::eInterpolative:
			attach.blendEnable = true;
			attach.srcColorBlendFactor = ashes::BlendFactor::eSrcColour;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcColour;
			break;

		default:
			attach.blendEnable = true;
			attach.srcColorBlendFactor = ashes::BlendFactor::eSrcColour;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcColour;
			break;
		}

		switch ( alphaBlendMode )
		{
		case BlendMode::eNoBlend:
			attach.srcAlphaBlendFactor = ashes::BlendFactor::eOne;
			attach.dstAlphaBlendFactor = ashes::BlendFactor::eZero;
			break;

		case BlendMode::eAdditive:
			attach.blendEnable = true;
			attach.srcAlphaBlendFactor = ashes::BlendFactor::eOne;
			attach.dstAlphaBlendFactor = ashes::BlendFactor::eOne;
			break;

		case BlendMode::eMultiplicative:
			attach.blendEnable = true;
			attach.srcAlphaBlendFactor = ashes::BlendFactor::eZero;
			attach.dstAlphaBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			attach.srcColorBlendFactor = ashes::BlendFactor::eZero;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			break;

		case BlendMode::eInterpolative:
			attach.blendEnable = true;
			attach.srcAlphaBlendFactor = ashes::BlendFactor::eSrcAlpha;
			attach.dstAlphaBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			attach.srcColorBlendFactor = ashes::BlendFactor::eSrcAlpha;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			break;

		default:
			attach.blendEnable = true;
			attach.srcAlphaBlendFactor = ashes::BlendFactor::eSrcAlpha;
			attach.dstAlphaBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			attach.srcColorBlendFactor = ashes::BlendFactor::eSrcAlpha;
			attach.dstColorBlendFactor = ashes::BlendFactor::eInvSrcAlpha;
			break;
		}

		ashes::ColourBlendState state;

		for ( auto i = 0u; i < attachesCount; ++i )
		{
			state.attachs.push_back( attach );
		}

		return state;
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, BillboardRenderNode & node )
	{
		auto & layout = descriptorPool.getLayout();
		node.uboDescriptorSet = descriptorPool.createDescriptorSet( 0u );
		getEngine()->getMaterialCache().getPassBuffer().createBinding( *node.uboDescriptorSet
			, layout.getBinding( PassBufferIndex ) );

		if ( node.pipeline.getFlags().texturesCount )
		{
			getEngine()->getMaterialCache().getTextureBuffer().createBinding( *node.uboDescriptorSet
				, layout.getBinding( TexturesBufferIndex ) );
		}

		if ( checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eLighting ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( LightBufferIndex )
				, node.sceneNode.getScene()->getLightCache().getBuffer()
				, node.sceneNode.getScene()->getLightCache().getView() );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( MatrixUbo::BindingPoint )
			, m_matrixUbo.getUbo() );
		node.uboDescriptorSet->createBinding( layout.getBinding( SceneUbo::BindingPoint )
			, m_sceneUbo.getUbo() );

		if ( !checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eInstantiation ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( ModelMatrixUbo::BindingPoint )
				, node.modelMatrixUbo.buffer->getBuffer()
				, node.modelMatrixUbo.offset
				, 1u );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( ModelUbo::BindingPoint )
			, node.modelUbo.buffer->getBuffer()
			, node.modelUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( BillboardUbo::BindingPoint )
			, node.billboardUbo.buffer->getBuffer()
			, node.billboardUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( TexturesUbo::BindingPoint )
			, node.texturesUbo.buffer->getBuffer()
			, node.texturesUbo.offset
			, 1u );
		doFillUboDescriptor( layout, node );
		node.uboDescriptorSet->update();
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, MorphingRenderNode & node )
	{
		auto & layout = descriptorPool.getLayout();
		node.uboDescriptorSet = descriptorPool.createDescriptorSet( 0u );
		getEngine()->getMaterialCache().getPassBuffer().createBinding( *node.uboDescriptorSet
			, layout.getBinding( PassBufferIndex ) );

		if ( node.pipeline.getFlags().texturesCount )
		{
			getEngine()->getMaterialCache().getTextureBuffer().createBinding( *node.uboDescriptorSet
				, layout.getBinding( TexturesBufferIndex ) );
		}

		if ( checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eLighting ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( LightBufferIndex )
				, node.sceneNode.getScene()->getLightCache().getBuffer()
				, node.sceneNode.getScene()->getLightCache().getView() );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( MatrixUbo::BindingPoint )
			, m_matrixUbo.getUbo() );
		node.uboDescriptorSet->createBinding( layout.getBinding( SceneUbo::BindingPoint )
			, m_sceneUbo.getUbo() );

		if ( !checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eInstantiation ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( ModelMatrixUbo::BindingPoint )
				, node.modelMatrixUbo.buffer->getBuffer()
				, node.modelMatrixUbo.offset
				, 1u );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( ModelUbo::BindingPoint )
			, node.modelUbo.buffer->getBuffer()
			, node.modelUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( MorphingUbo::BindingPoint )
			, node.morphingUbo.buffer->getBuffer()
			, node.morphingUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( TexturesUbo::BindingPoint )
			, node.texturesUbo.buffer->getBuffer()
			, node.texturesUbo.offset
			, 1u );
		doFillUboDescriptor( layout, node );
		node.uboDescriptorSet->update();
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SkinningRenderNode & node )
	{
		auto & layout = descriptorPool.getLayout();
		node.uboDescriptorSet = descriptorPool.createDescriptorSet( 0u );
		getEngine()->getMaterialCache().getPassBuffer().createBinding( *node.uboDescriptorSet
			, layout.getBinding( PassBufferIndex ) );

		if ( node.pipeline.getFlags().texturesCount )
		{
			getEngine()->getMaterialCache().getTextureBuffer().createBinding( *node.uboDescriptorSet
				, layout.getBinding( TexturesBufferIndex ) );
		}

		if ( checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eLighting ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( LightBufferIndex )
				, node.sceneNode.getScene()->getLightCache().getBuffer()
				, node.sceneNode.getScene()->getLightCache().getView() );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( MatrixUbo::BindingPoint )
			, m_matrixUbo.getUbo() );
		node.uboDescriptorSet->createBinding( layout.getBinding( SceneUbo::BindingPoint )
			, m_sceneUbo.getUbo() );

		if ( !checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eInstantiation ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( ModelMatrixUbo::BindingPoint )
				, node.modelMatrixUbo.buffer->getBuffer()
				, node.modelMatrixUbo.offset
				, 1u );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( ModelUbo::BindingPoint )
			, node.modelUbo.buffer->getBuffer()
			, node.modelUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( TexturesUbo::BindingPoint )
			, node.texturesUbo.buffer->getBuffer()
			, node.texturesUbo.offset
			, 1u );

		if ( checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eInstantiation ) )
		{
			node.data.getInstantiatedBones().getInstancedBonesBuffer().createBinding( *node.uboDescriptorSet
				, layout.getBinding( SkinningUbo::BindingPoint ) );
		}
		else
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( SkinningUbo::BindingPoint )
				, node.skinningUbo.buffer->getBuffer()
				, node.skinningUbo.offset
				, 1u );
		}

		doFillUboDescriptor( layout, node );
		node.uboDescriptorSet->update();
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, StaticRenderNode & node )
	{
		auto & layout = descriptorPool.getLayout();
		node.uboDescriptorSet = descriptorPool.createDescriptorSet( 0u );
		getEngine()->getMaterialCache().getPassBuffer().createBinding( *node.uboDescriptorSet
			, layout.getBinding( PassBufferIndex ) );

		if ( node.pipeline.getFlags().texturesCount )
		{
			getEngine()->getMaterialCache().getTextureBuffer().createBinding( *node.uboDescriptorSet
				, layout.getBinding( TexturesBufferIndex ) );
		}

		if ( checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eLighting ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( LightBufferIndex )
				, node.sceneNode.getScene()->getLightCache().getBuffer()
				, node.sceneNode.getScene()->getLightCache().getView() );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( MatrixUbo::BindingPoint )
			, m_matrixUbo.getUbo() );
		node.uboDescriptorSet->createBinding( layout.getBinding( SceneUbo::BindingPoint )
			, m_sceneUbo.getUbo() );

		if ( !checkFlag( node.pipeline.getFlags().programFlags, ProgramFlag::eInstantiation ) )
		{
			node.uboDescriptorSet->createBinding( layout.getBinding( ModelMatrixUbo::BindingPoint )
				, node.modelMatrixUbo.buffer->getBuffer()
				, node.modelMatrixUbo.offset
				, 1u );
		}

		node.uboDescriptorSet->createBinding( layout.getBinding( ModelUbo::BindingPoint )
			, node.modelUbo.buffer->getBuffer()
			, node.modelUbo.offset
			, 1u );
		node.uboDescriptorSet->createBinding( layout.getBinding( TexturesUbo::BindingPoint )
			, node.texturesUbo.buffer->getBuffer()
			, node.texturesUbo.offset
			, 1u );
		doFillUboDescriptor( layout, node );
		node.uboDescriptorSet->update();
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SubmeshSkinninRenderNodesByPassMap & nodes )
	{
		for ( auto & passNodes : nodes )
		{
			for ( auto & submeshNodes : passNodes.second )
			{
				initialiseUboDescriptor( descriptorPool, submeshNodes.second.begin()->second );
			}
		}
	}

	void RenderPass::initialiseUboDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SubmeshStaticRenderNodesByPassMap & nodes )
	{
		for ( auto & passNodes : nodes )
		{
			for ( auto & submeshNodes : passNodes.second )
			{
				initialiseUboDescriptor( descriptorPool, submeshNodes.second.begin()->second );
			}
		}
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, BillboardRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		auto & layout = descriptorPool.getLayout();
		uint32_t index = MinTextureIndex;
		node.texDescriptorSet = descriptorPool.createDescriptorSet( 1u );
		doFillTextureDescriptor( layout, index, node, shadowMaps );
		node.texDescriptorSet->update();
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, MorphingRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		auto & layout = descriptorPool.getLayout();
		uint32_t index = MinTextureIndex;
		node.texDescriptorSet = descriptorPool.createDescriptorSet( 1u );
		doFillTextureDescriptor( layout, index, node, shadowMaps );
		node.texDescriptorSet->update();
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SkinningRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		auto & layout = descriptorPool.getLayout();
		uint32_t index = MinTextureIndex;
		node.texDescriptorSet = descriptorPool.createDescriptorSet( 1u );
		doFillTextureDescriptor( layout, index, node, shadowMaps );
		node.texDescriptorSet->update();
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, StaticRenderNode & node
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		auto & layout = descriptorPool.getLayout();
		uint32_t index = MinTextureIndex;
		node.texDescriptorSet = descriptorPool.createDescriptorSet( 1u );
		doFillTextureDescriptor( layout, index, node, shadowMaps );
		node.texDescriptorSet->update();
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SubmeshSkinninRenderNodesByPassMap & nodes
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		for ( auto & passNodes : nodes )
		{
			for ( auto & submeshNodes : passNodes.second )
			{
				Pass & pass = submeshNodes.second.begin()->second.passNode.pass;

				if ( submeshNodes.second.begin()->second.pipeline.hasDescriptorPool( 1u ) )
				{
					initialiseTextureDescriptor( descriptorPool
						, submeshNodes.second.begin()->second
						, shadowMaps );
				}
			}
		}
	}

	void RenderPass::initialiseTextureDescriptor( ashes::DescriptorSetPool const & descriptorPool
		, SubmeshStaticRenderNodesByPassMap & nodes
		, ShadowMapLightTypeArray const & shadowMaps )
	{
		for ( auto & passNodes : nodes )
		{
			for ( auto & submeshNodes : passNodes.second )
			{
				Pass & pass = submeshNodes.second.begin()->second.passNode.pass;

				if ( submeshNodes.second.begin()->second.pipeline.hasDescriptorPool( 1u ) )
				{
					initialiseTextureDescriptor( descriptorPool
						, submeshNodes.second.begin()->second
						, shadowMaps );
				}
			}
		}
	}

	PassRenderNode RenderPass::doCreatePassRenderNode( Pass & pass
		, RenderPipeline & pipeline )
	{
		return PassRenderNode
		{
			pass,
		};
	}

	SceneRenderNode RenderPass::doCreateSceneRenderNode( Scene & scene
		, RenderPipeline & pipeline )
	{
		return SceneRenderNode{};
	}

	ShaderProgramSPtr RenderPass::doGetProgram( PipelineFlags const & flags )const
	{
		return getEngine()->getShaderProgramCache().getAutomaticProgram( *this
			, flags );
	}

	uint32_t RenderPass::doCopyNodesMatrices( StaticRenderNodePtrArray const & renderNodes
		, std::vector< InstantiationData > & matrixBuffer )const
	{
		return copyNodesMatrices( renderNodes
			, matrixBuffer );
	}

	uint32_t RenderPass::doCopyNodesMatrices( StaticRenderNodePtrArray const & renderNodes
		, std::vector< InstantiationData > & matrixBuffer
		, RenderInfo & info )const
	{
		auto count = copyNodesMatrices( renderNodes
			, matrixBuffer );
		info.m_visibleObjectsCount += count;
		return count;
	}

	uint32_t RenderPass::doCopyNodesMatrices( SkinningRenderNodePtrArray const & renderNodes
		, std::vector< InstantiationData > & matrixBuffer )const
	{
		return copyNodesMatrices( renderNodes
			, matrixBuffer );
	}

	uint32_t RenderPass::doCopyNodesMatrices( SkinningRenderNodePtrArray const & renderNodes
		, std::vector< InstantiationData > & matrixBuffer
		, RenderInfo & info )const
	{
		auto count = copyNodesMatrices( renderNodes
			, matrixBuffer );
		info.m_visibleObjectsCount += count;
		return count;
	}

	uint32_t RenderPass::doCopyNodesBones( SkinningRenderNodePtrArray const & renderNodes
		, ShaderBuffer & bonesBuffer )const
	{
		uint32_t const mtxSize = sizeof( float ) * 16;
		uint32_t const stride = mtxSize * 400u;
		auto const count = std::min( uint32_t( bonesBuffer.getSize() / stride ), uint32_t( renderNodes.size() ) );
		CU_Require( count <= renderNodes.size() );
		auto buffer = bonesBuffer.getPtr();
		auto it = renderNodes.begin();
		auto i = 0u;

		while ( i < count )
		{
			auto & node = *it;
			node->skeleton.fillBuffer( buffer );
			buffer += stride;
			++i;
			++it;
		}

		bonesBuffer.update( 0u, stride * count );
		return count;
	}

	uint32_t RenderPass::doCopyNodesBones( SkinningRenderNodePtrArray const & renderNodes
		, ShaderBuffer & bonesBuffer
		, RenderInfo & info )const
	{
		auto count = doCopyNodesBones( renderNodes, bonesBuffer );
		info.m_visibleObjectsCount += count;
		return count;
	}

	void RenderPass::doUpdate( SubmeshStaticRenderNodesPtrByPipelineMap & nodes )const
	{
		doTraverseNodes( *this
			, nodes
			, [this]( RenderPipeline & pipeline
				, Pass & pass
				, Submesh & submesh
				, InstantiationComponent & instantiation
				, StaticRenderNodePtrArray & renderNodes )
			{
				auto it = instantiation.find( pass.getOwner()->shared_from_this() );

				if ( !renderNodes.empty()
					&& it != instantiation.end()
					&& it->second.buffer )
				{
					doCopyNodesMatrices( renderNodes
						, it->second.data );
				}
			} );
	}

	void RenderPass::doUpdate( SubmeshStaticRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doTraverseNodes( *this
			, nodes
			, [this, &info]( RenderPipeline & pipeline
				, Pass & pass
				, Submesh & submesh
				, InstantiationComponent & instantiation
				, StaticRenderNodePtrArray & renderNodes )
			{
				auto it = instantiation.find( pass.getOwner()->shared_from_this() );

				if ( !renderNodes.empty()
					&& it != instantiation.end()
					&& it->second.buffer )
				{
					uint32_t count = doCopyNodesMatrices( renderNodes
						, it->second.data
						, info );
					info.m_visibleFaceCount += submesh.getFaceCount() * count;
					info.m_visibleVertexCount += submesh.getPointsCount() * count;
					++info.m_drawCalls;
				}
			} );
	}

	void RenderPass::doUpdate( StaticRenderNodesPtrByPipelineMap & nodes )const
	{
		doRenderNonInstanced( *this
			, nodes );
	}

	void RenderPass::doUpdate( StaticRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doRenderNonInstanced( *this
			, nodes
			, info );
	}

	void RenderPass::doUpdate( SkinningRenderNodesPtrByPipelineMap & nodes )const
	{
		doRenderNonInstanced( *this
			, nodes );
	}

	void RenderPass::doUpdate( SkinningRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doRenderNonInstanced( *this
			, nodes
			, info );
	}

	void RenderPass::doUpdate( SubmeshSkinningRenderNodesPtrByPipelineMap & nodes )const
	{
		doTraverseNodes( *this
			, nodes
			, [this]( RenderPipeline & pipeline
				, Pass & pass
				, Submesh & submesh
				, InstantiationComponent & instantiation
				, SkinningRenderNodePtrArray & renderNodes )
			{
				auto & instantiatedBones = submesh.getInstantiatedBones();
				auto it = instantiation.find( pass.getOwner()->shared_from_this() );

				if ( !renderNodes.empty()
					&& it != instantiation.end()
					&& it->second.buffer
					&& instantiatedBones.hasInstancedBonesBuffer() )
				{
					uint32_t count1 = doCopyNodesMatrices( renderNodes, it->second.data );
					uint32_t count2 = doCopyNodesBones( renderNodes, instantiatedBones.getInstancedBonesBuffer() );
					CU_Require( count1 == count2 );
				}
			} );
	}

	void RenderPass::doUpdate( SubmeshSkinningRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doTraverseNodes( *this
			, nodes
			, [this, &info]( RenderPipeline & pipeline
				, Pass & pass
				, Submesh & submesh
				, InstantiationComponent & instantiation
				, SkinningRenderNodePtrArray & renderNodes )
			{
				auto & instantiatedBones = submesh.getInstantiatedBones();
				auto it = instantiation.find( pass.getOwner()->shared_from_this() );

				if ( !renderNodes.empty()
					&& it != instantiation.end()
					&& it->second.buffer
					&& instantiatedBones.hasInstancedBonesBuffer() )
				{
					uint32_t count1 = doCopyNodesMatrices( renderNodes, it->second.data, info );
					uint32_t count2 = doCopyNodesBones( renderNodes, instantiatedBones.getInstancedBonesBuffer(), info );
					CU_Require( count1 == count2 );
					info.m_visibleFaceCount += submesh.getFaceCount() * count1;
					info.m_visibleVertexCount += submesh.getPointsCount() * count1;
					++info.m_drawCalls;
				}
			} );
	}

	void RenderPass::doUpdate( MorphingRenderNodesPtrByPipelineMap & nodes )const
	{
		doRenderNonInstanced( *this
			, nodes );
	}

	void RenderPass::doUpdate( MorphingRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doRenderNonInstanced( *this
			, nodes
			, info );
	}

	void RenderPass::doUpdate( BillboardRenderNodesPtrByPipelineMap & nodes )const
	{
		doRenderNonInstanced( *this
			, nodes );
	}

	void RenderPass::doUpdate( BillboardRenderNodesPtrByPipelineMap & nodes
		, RenderInfo & info )const
	{
		doRenderNonInstanced( *this
			, nodes
			, info );
	}

	void RenderPass::doUpdateUbos( Camera const & camera
		, Point2r const & jitter )
	{
		auto jitterProjSpace = jitter * 2.0_r;
		jitterProjSpace[0] /= camera.getWidth();
		jitterProjSpace[1] /= camera.getHeight();
		m_matrixUbo.update( camera.getView()
			, camera.getProjection()
			, jitterProjSpace );
	}

	std::map< PipelineFlags, RenderPipelineUPtr > & RenderPass::doGetFrontPipelines()
	{
#if C3D_InvertFrontAndBackPipelines
		return getEngine()->isTopDown()
			? m_frontPipelines
			: m_backPipelines;
#else
		return m_frontPipelines;
#endif
	}

	std::map< PipelineFlags, RenderPipelineUPtr > & RenderPass::doGetBackPipelines()
	{
#if C3D_InvertFrontAndBackPipelines
		return getEngine()->isTopDown()
			? m_backPipelines
			: m_frontPipelines;
#else
		return m_backPipelines;
#endif
	}

	std::map< PipelineFlags, RenderPipelineUPtr > const & RenderPass::doGetFrontPipelines()const
	{
#if C3D_InvertFrontAndBackPipelines
		return getEngine()->isTopDown()
			? m_frontPipelines
			: m_backPipelines;
#else
		return m_frontPipelines;
#endif
	}

	std::map< PipelineFlags, RenderPipelineUPtr > const & RenderPass::doGetBackPipelines()const
	{
#if C3D_InvertFrontAndBackPipelines
		return getEngine()->isTopDown()
			? m_backPipelines
			: m_frontPipelines;
#else
		return m_backPipelines;
#endif
	}

	void RenderPass::doPrepareFrontPipeline( ShaderProgramSPtr program
		, ashes::VertexLayoutCRefArray const & layouts
		, PipelineFlags const & flags )
	{
		auto & pipelines = doGetFrontPipelines();

		if ( pipelines.find( flags ) == pipelines.end() )
		{
			auto dsState = doCreateDepthStencilState( flags );
			auto bdState = doCreateBlendState( flags );
			auto & pipeline = *pipelines.emplace( flags
				, std::make_unique< RenderPipeline >( *getEngine()->getRenderSystem()
					, std::move( dsState )
					, ashes::RasterisationState{ 0u, false, false, ashes::PolygonMode::eFill, ashes::CullModeFlag::eFront }
					, std::move( bdState )
					, ashes::MultisampleState{}
					, program
					, flags ) ).first->second;
			pipeline.setVertexLayouts( layouts );
			pipeline.setViewport( { m_size.getWidth(), m_size.getHeight(), 0, 0 } );
			pipeline.setScissor( { 0, 0, m_size.getWidth(), m_size.getHeight() } );

			getEngine()->sendEvent( makeFunctorEvent( EventType::ePreRender
				, [this, &pipeline, flags]()
				{
					auto uboBindings = doCreateUboBindings( flags );
					auto texBindings = doCreateTextureBindings( flags );
					auto uboLayout = getCurrentDevice( *this ).createDescriptorSetLayout( std::move( uboBindings ) );
					auto texLayout = getCurrentDevice( *this ).createDescriptorSetLayout( std::move( texBindings ) );
					std::vector< ashes::DescriptorSetLayoutPtr > layouts;
					layouts.emplace_back( std::move( uboLayout ) );
					layouts.emplace_back( std::move( texLayout ) );
					pipeline.setDescriptorSetLayouts( std::move( layouts ) );
					pipeline.initialise( getRenderPass() );
				} ) );
		}
	}

	void RenderPass::doPrepareBackPipeline( ShaderProgramSPtr program
		, ashes::VertexLayoutCRefArray const & layouts
		, PipelineFlags const & flags )
	{
		auto & pipelines = doGetBackPipelines();

		if ( pipelines.find( flags ) == pipelines.end() )
		{
			auto dsState = doCreateDepthStencilState( flags );
			auto bdState = doCreateBlendState( flags );
			auto & pipeline = *pipelines.emplace( flags
				, std::make_unique< RenderPipeline >( *getEngine()->getRenderSystem()
					, std::move( dsState )
					, ashes::RasterisationState{ 0u, false, false, ashes::PolygonMode::eFill, ashes::CullModeFlag::eBack }
					, std::move( bdState )
					, ashes::MultisampleState{}
					, program
					, flags ) ).first->second;
			pipeline.setVertexLayouts( layouts );
			pipeline.setViewport( { m_size.getWidth(), m_size.getHeight(), 0, 0 } );
			pipeline.setScissor( { 0, 0, m_size.getWidth(), m_size.getHeight() } );

			getEngine()->sendEvent( makeFunctorEvent( EventType::ePreRender
				, [this, &pipeline, flags]()
				{
					auto uboBindings = doCreateUboBindings( flags );
					auto texBindings = doCreateTextureBindings( flags );
					auto uboLayout = getCurrentDevice( *this ).createDescriptorSetLayout( std::move( uboBindings ) );
					auto texLayout = getCurrentDevice( *this ).createDescriptorSetLayout( std::move( texBindings ) );
					std::vector< ashes::DescriptorSetLayoutPtr > layouts;
					layouts.emplace_back( std::move( uboLayout ) );
					layouts.emplace_back( std::move( texLayout ) );
					pipeline.setDescriptorSetLayouts( std::move( layouts ) );
					pipeline.initialise( getRenderPass() );
				} ) );
		}
	}

	ashes::DescriptorSetLayoutBindingArray RenderPass::doCreateUboBindings( PipelineFlags const & flags )const
	{
		ashes::DescriptorSetLayoutBindingArray uboBindings;
		uboBindings.emplace_back( getEngine()->getMaterialCache().getPassBuffer().createLayoutBinding() );
		uboBindings.emplace_back( getEngine()->getMaterialCache().getTextureBuffer().createLayoutBinding() );

		if ( checkFlag( flags.programFlags, ProgramFlag::eLighting ) )
		{
			uboBindings.emplace_back( LightBufferIndex, ashes::DescriptorType::eUniformTexelBuffer, ashes::ShaderStageFlag::eFragment );
		}

		uboBindings.emplace_back( MatrixUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex );
		uboBindings.emplace_back( SceneUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex | ashes::ShaderStageFlag::eFragment );
		uboBindings.emplace_back( ModelMatrixUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex );
		uboBindings.emplace_back( ModelUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex | ashes::ShaderStageFlag::eFragment );
		uboBindings.emplace_back( TexturesUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eFragment );

		if ( checkFlag( flags.programFlags, ProgramFlag::eSkinning ) )
		{
			uboBindings.push_back( SkinningUbo::createLayoutBinding( SkinningUbo::BindingPoint, flags.programFlags ) );
		}

		if ( checkFlag( flags.programFlags, ProgramFlag::eMorphing ) )
		{
			uboBindings.emplace_back( MorphingUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex );
		}

		if ( checkFlag( flags.programFlags, ProgramFlag::ePicking ) )
		{
			uboBindings.emplace_back( PickingUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eFragment );
		}

		if ( checkFlag( flags.programFlags, ProgramFlag::eBillboards ) )
		{
			uboBindings.emplace_back( BillboardUbo::BindingPoint, ashes::DescriptorType::eUniformBuffer, ashes::ShaderStageFlag::eVertex );
		}

		return uboBindings;
	}

	ShaderPtr RenderPass::doGetVertexShaderSource( PipelineFlags const & flags )const
	{
		using namespace sdw;
		VertexWriter writer;
		// Vertex inputs
		auto position = writer.declInput< Vec4 >( cuT( "position" )
			, RenderPass::VertexInputs::PositionLocation );
		auto normal = writer.declInput< Vec3 >( cuT( "normal" )
			, RenderPass::VertexInputs::NormalLocation );
		auto tangent = writer.declInput< Vec3 >( cuT( "tangent" )
			, RenderPass::VertexInputs::TangentLocation );
		auto uv = writer.declInput< Vec3 >( cuT( "uv" )
			, RenderPass::VertexInputs::TextureLocation );
		auto bone_ids0 = writer.declInput< IVec4 >( cuT( "bone_ids0" )
			, RenderPass::VertexInputs::BoneIds0Location
			, checkFlag( flags.programFlags, ProgramFlag::eSkinning ) );
		auto bone_ids1 = writer.declInput< IVec4 >( cuT( "bone_ids1" )
			, RenderPass::VertexInputs::BoneIds1Location
			, checkFlag( flags.programFlags, ProgramFlag::eSkinning ) );
		auto weights0 = writer.declInput< Vec4 >( cuT( "weights0" )
			, RenderPass::VertexInputs::Weights0Location
			, checkFlag( flags.programFlags, ProgramFlag::eSkinning ) );
		auto weights1 = writer.declInput< Vec4 >( cuT( "weights1" )
			, RenderPass::VertexInputs::Weights1Location
			, checkFlag( flags.programFlags, ProgramFlag::eSkinning ) );
		auto transform = writer.declInput< Mat4 >( cuT( "transform" )
			, RenderPass::VertexInputs::TransformLocation
			, checkFlag( flags.programFlags, ProgramFlag::eInstantiation ) );
		auto material = writer.declInput< Int >( cuT( "material" )
			, RenderPass::VertexInputs::MaterialLocation
			, checkFlag( flags.programFlags, ProgramFlag::eInstantiation ) );
		auto position2 = writer.declInput< Vec4 >( cuT( "position2" )
			, RenderPass::VertexInputs::Position2Location
			, checkFlag( flags.programFlags, ProgramFlag::eMorphing ) );
		auto normal2 = writer.declInput< Vec3 >( cuT( "normal2" )
			, RenderPass::VertexInputs::Normal2Location
			, checkFlag( flags.programFlags, ProgramFlag::eMorphing ) );
		auto tangent2 = writer.declInput< Vec3 >( cuT( "tangent2" )
			, RenderPass::VertexInputs::Tangent2Location
			, checkFlag( flags.programFlags, ProgramFlag::eMorphing ) );
		auto texture2 = writer.declInput< Vec3 >( cuT( "texture2" )
			, RenderPass::VertexInputs::Texture2Location
			, checkFlag( flags.programFlags, ProgramFlag::eMorphing ) );
		auto in = writer.getIn();

		UBO_MATRIX( writer, MatrixUbo::BindingPoint, 0 );
		UBO_SCENE( writer, SceneUbo::BindingPoint, 0 );
		UBO_MODEL_MATRIX( writer, ModelMatrixUbo::BindingPoint, 0 );
		UBO_MODEL( writer, ModelUbo::BindingPoint, 0 );
		auto skinningData = SkinningUbo::declare( writer, SkinningUbo::BindingPoint, 0, flags.programFlags );
		UBO_MORPHING( writer, MorphingUbo::BindingPoint, 0, flags.programFlags );

		// Outputs
		auto vtx_worldPosition = writer.declOutput< Vec3 >( cuT( "vtx_worldPosition" )
			, RenderPass::VertexOutputs::WorldPositionLocation );
		auto vtx_tangentSpaceFragPosition = writer.declOutput< Vec3 >( cuT( "vtx_tangentSpaceFragPosition" )
			, RenderPass::VertexOutputs::TangentSpaceFragPositionLocation );
		auto vtx_tangentSpaceViewPosition = writer.declOutput< Vec3 >( cuT( "vtx_tangentSpaceViewPosition" )
			, RenderPass::VertexOutputs::TangentSpaceViewPositionLocation );
		auto vtx_normal = writer.declOutput< Vec3 >( cuT( "vtx_normal" )
			, RenderPass::VertexOutputs::NormalLocation );
		auto vtx_tangent = writer.declOutput< Vec3 >( cuT( "vtx_tangent" )
			, RenderPass::VertexOutputs::TangentLocation );
		auto vtx_bitangent = writer.declOutput< Vec3 >( cuT( "vtx_bitangent" )
			, RenderPass::VertexOutputs::BitangentLocation );
		auto vtx_texture = writer.declOutput< Vec3 >( cuT( "vtx_texture" )
			, RenderPass::VertexOutputs::TextureLocation );
		auto vtx_instance = writer.declOutput< UInt >( cuT( "vtx_instance" )
			, RenderPass::VertexOutputs::InstanceLocation );
		auto vtx_material = writer.declOutput< UInt >( cuT( "vtx_material" )
			, RenderPass::VertexOutputs::MaterialLocation );
		auto out = writer.getOut();

		std::function< void() > main = [&]()
		{
			auto v4Vertex = writer.declLocale( cuT( "v4Vertex" )
				, vec4( position.xyz(), 1.0 ) );
			auto v4Normal = writer.declLocale( cuT( "v4Normal" )
				, vec4( normal, 0.0 ) );
			auto v4Tangent = writer.declLocale( cuT( "v4Tangent" )
				, vec4( tangent, 0.0 ) );
			auto v3Texture = writer.declLocale( cuT( "v3Texture" )
				, uv );
			auto curMtxModel = writer.declLocale< Mat4 >( cuT( "curMtxModel" ) );
			auto prvMtxModel = writer.declLocale< Mat4 >( cuT( "prvMtxModel" ) );

			if ( checkFlag( flags.programFlags, ProgramFlag::eSkinning ) )
			{
				curMtxModel = SkinningUbo::computeTransform( skinningData, writer, flags.programFlags );
				prvMtxModel = curMtxModel;
			}
			else if ( checkFlag( flags.programFlags, ProgramFlag::eInstantiation ) )
			{
				curMtxModel = transform;
				prvMtxModel = curMtxModel;
			}
			else
			{
				curMtxModel = c3d_curMtxModel;
				prvMtxModel = c3d_prvMtxModel;
			}

			auto curMtxNormal = writer.declLocale( cuT( "curMtxNormal" )
				, transpose( inverse( mat3( curMtxModel ) ) ) );
			auto prvMtxNormal = writer.declLocale( cuT( "prvMtxNormal" )
				, transpose( inverse( mat3( prvMtxModel ) ) ) );

			if ( checkFlag( flags.programFlags, ProgramFlag::eInstantiation ) )
			{
				vtx_material = writer.cast< UInt >( material );
			}
			else
			{
				vtx_material = writer.cast< UInt >( c3d_materialIndex );
			}

			if ( checkFlag( flags.programFlags, ProgramFlag::eMorphing ) )
			{
				auto time = writer.declLocale( cuT( "time" )
					, vec3( 1.0_f - c3d_time ) );
				v4Vertex = vec4( sdw::fma( v4Vertex.xyz(), time, position2.xyz() * c3d_time ), 1.0 );
				v4Normal = vec4( sdw::fma( v4Normal.xyz(), time, normal2.xyz() * c3d_time ), 1.0 );
				v4Tangent = vec4( sdw::fma( v4Tangent.xyz(), time, tangent2.xyz() * c3d_time ), 1.0 );
				v3Texture = sdw::fma( v3Texture, time, texture2 * c3d_time );
			}

			vtx_texture = v3Texture;
			v4Vertex = curMtxModel * v4Vertex;
			vtx_worldPosition = v4Vertex.xyz();
			v4Vertex = c3d_curView * v4Vertex;
			auto mtxNormal = writer.getVariable< Mat3 >( cuT( "mtxNormal" ) );

			if ( checkFlag( flags.programFlags, ProgramFlag::eInvertNormals ) )
			{
				vtx_normal = normalize( writer.paren( mtxNormal * -v4Normal.xyz() ) );
			}
			else
			{
				vtx_normal = normalize( writer.paren( mtxNormal * v4Normal.xyz() ) );
			}

			vtx_tangent = normalize( writer.paren( mtxNormal * v4Tangent.xyz() ) );
			vtx_tangent = normalize( sdw::fma( -vtx_normal, vec3( dot( vtx_tangent, vtx_normal ) ), vtx_tangent ) );
			vtx_bitangent = cross( vtx_normal, vtx_tangent );
			vtx_instance = writer.cast< UInt >( in.gl_InstanceID );
			out.gl_out.gl_Position = c3d_projection * v4Vertex;

			auto tbn = writer.declLocale( cuT( "tbn" )
				, transpose( mat3( vtx_tangent, vtx_bitangent, vtx_normal ) ) );
			vtx_tangentSpaceFragPosition = tbn * vtx_worldPosition;
			vtx_tangentSpaceViewPosition = tbn * c3d_cameraPosition.xyz();
		};

		writer.implementFunction< sdw::Void >( cuT( "main" ), main );
		return std::make_unique< sdw::Shader >( std::move( writer.getShader() ) );
	}
}
