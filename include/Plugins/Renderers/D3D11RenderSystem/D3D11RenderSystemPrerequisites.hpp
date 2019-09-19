/*
See LICENSE file in root folder
*/
#ifndef ___D3D11_RENDER_SYSTEM_PREREQUISITES_H___
#define ___D3D11_RENDER_SYSTEM_PREREQUISITES_H___

#include <CastorUtils/CastorUtilsPrerequisites.hpp>
#include <Castor3D/Castor3DPrerequisites.hpp>

#ifdef CU_PlatformWindows
#	ifdef D3D11RenderSystem_EXPORTS
#		define C3D_D3D11_API __declspec( dllexport )
#	else
#		define C3D_D3D11_API __declspec( dllimport )
#	endif
#else
#	define C3D_D3D11_API
#endif

namespace D3D11Render
{
	class RenderSystem;
}

#endif