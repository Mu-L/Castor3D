#include "Castor3D/Mesh/Importer.hpp"

#include "Castor3D/Engine.hpp"

#include "Castor3D/Event/Frame/FrameListener.hpp"
#include "Castor3D/Event/Frame/InitialiseEvent.hpp"
#include "Castor3D/Material/Material.hpp"
#include "Castor3D/Material/Pass.hpp"
#include "Castor3D/Mesh/Mesh.hpp"
#include "Castor3D/Mesh/Submesh.hpp"
#include "Castor3D/Mesh/Vertex.hpp"
#include "Castor3D/Scene/Geometry.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Texture/TextureConfiguration.hpp"
#include "Castor3D/Texture/TextureView.hpp"
#include "Castor3D/Texture/TextureLayout.hpp"

using namespace castor;

namespace castor3d
{
	Importer::Importer( Engine & engine )
		: OwnedBy< Engine >( engine )
		, m_fileName()
	{
	}

	bool Importer::importScene( Scene & scene
		, Path const & fileName
		, Parameters const & parameters )
	{
		bool splitSubmeshes = false;
		m_parameters.get( cuT( "split_mesh" ), splitSubmeshes );
		m_fileName = fileName;
		m_filePath = m_fileName.getPath();
		m_parameters = parameters;
		m_nodes.clear();
		m_geometries.clear();
		bool result = doImportScene( scene );

		if ( result )
		{
			for ( auto it : m_geometries )
			{
				auto mesh = it.second->getMesh();
				mesh->computeContainers();
				mesh->computeNormals();

				for ( auto submesh : *mesh )
				{
					scene.getListener().postEvent( makeInitialiseEvent( *submesh ) );
				}
			}
		}

		return result;
	}

	bool Importer::importMesh( Mesh & mesh
		, Path const & fileName
		, Parameters const & parameters
		, bool initialise )
	{
		bool splitSubmeshes = false;
		m_parameters.get( cuT( "split_mesh" ), splitSubmeshes );
		m_fileName = fileName;
		m_filePath = m_fileName.getPath();
		m_parameters = parameters;
		m_nodes.clear();
		m_geometries.clear();
		bool result = true;

		if ( !mesh.getSubmeshCount() )
		{
			result = doImportMesh( mesh );

			if ( result && initialise )
			{
				float scale = 1.0f;

				if ( m_parameters.get( cuT( "rescale" ), scale )
					&& std::abs( scale - 1.0f ) > 0.01f )
				{
					for ( auto submesh : mesh )
					{
						for ( auto & vertex : submesh->getPoints() )
						{
							vertex.pos *= scale;
						}
					}
				}

				mesh.computeContainers();

				for ( auto submesh : mesh )
				{
					mesh.getScene()->getListener().postEvent( makeInitialiseEvent( *submesh ) );
				}
			}
		}
		else
		{
			for ( auto submesh : mesh )
			{
				submesh->setMaterial( nullptr, submesh->getDefaultMaterial(), false );
			}
		}

		return result;
	}

	TextureUnitSPtr Importer::loadTexture( Path const & path
		, TextureConfiguration const & config )const
	{
		TextureUnitSPtr result;
		Path relative;
		Path folder;

		if ( File::fileExists( path ) )
		{
			relative = path;
		}
		else if ( File::fileExists( m_filePath / path ) )
		{
			auto fullPath = m_filePath / path;
			folder = fullPath.getPath();
			relative = fullPath.getFileName( true );;
		}
		else
		{
			PathArray files;
			String fileName = path.getFileName( true );
			File::listDirectoryFiles( m_filePath, files, true );
			auto it = std::find_if( files.begin()
				, files.end()
				, [&fileName]( Path const & file )
				{
					return file.getFileName( true ) == fileName
						|| file.getFileName( true ).find( fileName ) == 0;
				} );

			folder = m_filePath;

			if ( it != files.end() )
			{
				relative = *it;
				relative = Path{ relative.substr( folder.size() + 1 ) };
			}
			else
			{
				relative = Path{ fileName };
			}
		}

		if ( File::fileExists( folder / relative ) )
		{
			try
			{
				result = std::make_shared< TextureUnit >( *getEngine() );
				result->setAutoMipmaps( true );
				ashes::ImageCreateInfo createInfo{};
				createInfo.flags = 0u;
				createInfo.arrayLayers = 1u;
				createInfo.imageType = ashes::TextureType::e2D;
				createInfo.initialLayout = ashes::ImageLayout::eUndefined;
				createInfo.mipLevels = 20u;
				createInfo.samples = ashes::SampleCountFlag::e1;
				createInfo.sharingMode = ashes::SharingMode::eExclusive;
				createInfo.tiling = ashes::ImageTiling::eOptimal;
				createInfo.usage = ashes::ImageUsageFlag::eSampled
					| ashes::ImageUsageFlag::eTransferDst;
				auto texture = std::make_shared < TextureLayout >( *getEngine()->getRenderSystem()
					, createInfo
					, ashes::MemoryPropertyFlag::eDeviceLocal
					, relative );
				texture->setSource( folder
					, relative );
				result->setTexture( texture );
				result->setConfiguration( config );
			}
			catch ( std::exception & exc )
			{
				result.reset();
				Logger::logWarning( makeStringStream() << cuT( "Error encountered while loading texture file " ) << path << cuT( ":\n" ) << exc.what() );
			}
			catch ( ... )
			{
				result.reset();
				Logger::logWarning( cuT( "Unknown error encountered while loading texture file " ) + path );
			}
		}
		else
		{
			Logger::logWarning( makeStringStream() << cuT( "Couldn't load texture file " ) << path << cuT( ":\nFile does not exist." ) );
		}

		return result;
	}

	TextureUnitSPtr Importer::loadTexture( Path const & path
		, TextureConfiguration const & config
		, Pass & pass )const
	{
		auto result = loadTexture( path
			, config );

		if ( result )
		{
			pass.addTextureUnit( result );
		}

		return result;
	}
}
