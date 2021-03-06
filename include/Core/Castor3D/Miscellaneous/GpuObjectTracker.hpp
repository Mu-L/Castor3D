/*
See LICENSE file in root folder
*/
#ifndef ___C3D_GpuObjectTracker_H___
#define ___C3D_GpuObjectTracker_H___

#ifndef C3D_TRACE_OBJECTS
#	ifndef NDEBUG
#		define C3D_TRACE_OBJECTS 1
#	else
#		define C3D_TRACE_OBJECTS 0
#	endif
#endif

#if C3D_TRACE_OBJECTS

#include "MiscellaneousModule.hpp"

#include <CastorUtils/Design/Named.hpp>

#include <list>

namespace castor3d
{
	class GpuObjectTracker
	{
	public:
		struct ObjectDeclaration
		{
			uint32_t m_id;
			std::string m_name;
			void * m_object;
			std::string m_file;
			int m_line;
			std::string m_stack;
		};

	public:
		C3D_API bool track( void * object, std::string const & type, std::string const & file, int line, std::string & name );
		C3D_API bool track( castor::Named * object, std::string const & type, std::string const & file, int line, std::string & name );
		C3D_API bool untrack( void * object, ObjectDeclaration & declaration );
		C3D_API void reportTracked();

	private:
		uint32_t m_id = 0;
		std::list< ObjectDeclaration > m_allocated;
	};
}

#endif

#endif
