/*
See LICENSE file in root folder
*/
#ifndef ___C3D_TextBackground_H___
#define ___C3D_TextBackground_H___

#include "Castor3D/Scene/Background/Background.hpp"

#include <CastorUtils/Data/TextWriter.hpp>

namespace castor
{
	template<>
	class TextWriter< castor3d::SceneBackground >
		: public TextWriterT< castor3d::SceneBackground >
	{
	public:
		C3D_API explicit TextWriter( castor::String const & tabs );
		C3D_API bool operator()( castor3d::SceneBackground const & overlay
			, castor::TextFile & file );
	};
}

#endif