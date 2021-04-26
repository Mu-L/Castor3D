/*
See LICENSE file in root folder
*/
#ifndef ___C3D_ShadowMapUbo_H___
#define ___C3D_ShadowMapUbo_H___

#include "UbosModule.hpp"

#include "Castor3D/Buffer/UniformBufferOffset.hpp"
#include "Castor3D/Scene/Light/LightModule.hpp"

namespace castor3d
{
	namespace shader
	{
		struct ShadowMapData
			: public sdw::StructInstance
		{
			C3D_API ShadowMapData( sdw::ShaderWriter & writer
				, ast::expr::ExprPtr expr
				, bool enabled );
			C3D_API ShadowMapData & operator=( ShadowMapData const & rhs );

			C3D_API static ast::type::StructPtr makeType( ast::type::TypesCache & cache );
			C3D_API static std::unique_ptr< sdw::Struct > declare( sdw::ShaderWriter & writer );

			C3D_API sdw::Vec4 worldToView( sdw::Vec4 const & pos )const;
			C3D_API sdw::Vec4 viewToProj( sdw::Vec4 const & pos )const;
			C3D_API sdw::Float getLinearisedDepth( sdw::Vec3 const & pos )const;
			C3D_API DirectionalLight getDirectionalLight( LightingModel const & lighting )const;
			C3D_API PointLight getPointLight( LightingModel const & lighting )const;
			C3D_API SpotLight getSpotLight( LightingModel const & lighting )const;

		private:
			using sdw::StructInstance::getMember;
			using sdw::StructInstance::getMemberArray;

		private:
			sdw::Mat4 m_lightProjection;
			sdw::Mat4 m_lightView;
			sdw::Vec4 m_lightPosFarPlane;
			sdw::UInt m_lightIndex;
		};
	}

	class ShadowMapUbo
	{
	public:
		using Configuration = ShadowMapUboConfiguration;

	public:
		/**
		 *\~english
		 *\name			Copy/Move construction/assignment operation.
		 *\~french
		 *\name			Constructeurs/Opérateurs d'affectation par copie/déplacement.
		 */
		/**@{*/
		C3D_API ShadowMapUbo( ShadowMapUbo const & ) = delete;
		C3D_API ShadowMapUbo & operator=( ShadowMapUbo const & ) = delete;
		C3D_API ShadowMapUbo( ShadowMapUbo && ) = default;
		C3D_API ShadowMapUbo & operator=( ShadowMapUbo && ) = delete;
		/**@}*/
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	engine	The engine.
		 *\~french
		 *\brief		Constructeur.
		 *\param[in]	engine	Le moteur.
		 */
		C3D_API explicit ShadowMapUbo( Engine & engine );
		/**
		 *\~english
		 *\brief		Destructor
		 *\~french
		 *\brief		Destructeur
		 */
		C3D_API ~ShadowMapUbo();
		/**
		 *\~english
		 *\brief		Initialises the UBO.
		 *\param[in]	device	The GPU device.
		 *\~french
		 *\brief		Initialise l'UBO.
		 *\param[in]	device	Le device GPU.
		 */
		C3D_API void initialise( RenderDevice const & device );
		/**
		 *\~english
		 *\brief		Cleanup function.
		 *\param[in]	device	The GPU device.
		 *\~french
		 *\brief		Fonction de nettoyage.
		 *\param[in]	device	Le device GPU.
		 */
		C3D_API void cleanup( RenderDevice const & device );
		/**
		 *\~english
		 *\brief		Updates the UBO from given values.
		 *\param[in]	light	The light source from which the shadow map is generated.
		 *\param[in]	index	The shadow pass index.
		 *\~french
		 *\brief		Met à jour l'UBO avec les valeurs données.
		 *\param[in]	light	La source lumineuse depuis laquelle la shadow map est générée.
		 *\param[in]	index	L'index de la passe d'ombres.
		 */
		C3D_API void update( Light const & light
			, uint32_t index );

		void createSizedBinding( ashes::DescriptorSet & descriptorSet
			, VkDescriptorSetLayoutBinding const & layoutBinding )const
		{
			return m_ubo.createSizedBinding( descriptorSet, layoutBinding );
		}

		UniformBufferOffsetT< Configuration > const & getUbo()const
		{
			return m_ubo;
		}

	public:
		C3D_API static castor::String const BufferShadowMap;
		C3D_API static castor::String const ShadowMapData;

	private:
		Engine & m_engine;
		UniformBufferOffsetT< Configuration > m_ubo;
	};
}

#define UBO_SHADOWMAP( writer, binding, set )\
	sdw::Ubo shadowMapCfg{ writer\
		, castor3d::ShadowMapUbo::BufferShadowMap\
		, binding\
		, set };\
	auto c3d_shadowMapData = shadowMapCfg.declStructMember< castor3d::shader::ShadowMapData >( castor3d::ShadowMapUbo::ShadowMapData );\
	shadowMapCfg.end()

#endif
