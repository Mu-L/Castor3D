/*
See LICENSE file in root folder
*/
#ifndef ___C3D_SHADER_CACHE_H___
#define ___C3D_SHADER_CACHE_H___

#include "Castor3D/Castor3DPrerequisites.hpp"

namespace castor3d
{
	/*!
	\author 	Sylvain DOREMUS
	\date		14/02/2010
	\~english
	\brief		Cache used to hold the shader programs. Holds it, destroys it during a rendering loop
	\~french
	\brief		Cache utilisé pour garder les programmes de shaders. Il les garde et permet leur destruction au cours d'une boucle de rendu
	*/
	class ShaderProgramCache
		: public castor::OwnedBy< Engine >
	{
		using ShaderProgramPtrArray = std::vector< ShaderProgramSPtr >;

	public:
		/**
		 *\~english
		 *\brief		Constructor
		 *\param[in]	engine	The engine
		 *\~french
		 *\brief		Constructeur
		 *\param[in]	engine	Le moteur
		 */
		C3D_API explicit ShaderProgramCache( Engine & engine );
		/**
		 *\~english
		 *\brief		Destructor
		 *\~french
		 *\brief		Destructeur
		 */
		C3D_API ~ShaderProgramCache();
		/**
		 *\~english
		 *\brief		Destroys all the shaders of the array of shaders to destroy
		 *\~french
		 *\brief		Détruit tous les shaders du tableau de shaders à détruire
		 */
		C3D_API void clear();
		/**
		 *\~english
		 *\brief		Cleans up all the shaders.
		 *\~french
		 *\brief		Nettoie tous les shaders.
		 */
		C3D_API void cleanup();
		/**
		 *\~english
		 *\brief		Creates a new program.
		 *\param[in]	initialise	Tells if we want the program to be initialised.
		 *\return		The newly created program.
		 *\~french
		 *\brief		Crée un nouveau programme.
		 *\param[in]	initialise	Dit si on veut que le programme soit initialisé.
		 *\return		Le programme créé.
		 */
		C3D_API ShaderProgramSPtr getNewProgram( bool initialise );
		/**
		 *\~english
		 *\brief		Looks for an automatically generated program corresponding to given flags.
		 *\remarks		If none exists it is created.
		 *\param[in]	renderPass	The pass from which the program code is retrieved.
		 *\param[in]	flags		The pipeline flags.
		 *\return		The found or created program.
		 *\~french
		 *\brief		Cherche un programme automatiquement généré correspondant aux flags donnés.
		 *\param[in]	renderPass	La passe a partir de laquelle est récupéré le code du programme.
		 *\param[in]	flags		Les flags de pipeline.
		 *\return		Le programme trouvé ou créé.
		 */
		C3D_API ShaderProgramSPtr getAutomaticProgram( RenderPass const & renderPass
			, PipelineFlags const & flags );
		/**
		 *\~english
		 *\brief		Locks the collection mutex
		 *\~french
		 *\brief		Locke le mutex de la collection
		 */
		inline void lock()const
		{
			m_mutex.lock();
		}
		/**
		 *\~english
		 *\brief		Unlocks the collection mutex
		 *\~french
		 *\brief		Délocke le mutex de la collection
		 */
		inline void unlock()const
		{
			m_mutex.unlock();
		}
		/**
		 *\~english
		 *\brief		Retrieves an iterator to the beginning of the shaders list.
		 *\return		The iterator
		 *\~french
		 *\brief		Récupère un itérateur sur le début de la liste de shaders.
		 *\return		L4itérateur
		 */
		inline ShaderProgramPtrArray::iterator begin()
		{
			return m_arrayPrograms.begin();
		}
		/**
		 *\~english
		 *\brief		Retrieves an iterator to the beginning of the shaders list.
		 *\return		The iterator
		 *\~french
		 *\brief		Récupère un itérateur sur le début de la liste de shaders.
		 *\return		L4itérateur
		 */
		inline ShaderProgramPtrArray::const_iterator begin()const
		{
			return m_arrayPrograms.begin();
		}
		/**
		 *\~english
		 *\brief		Retrieves an iterator to the end of the shaders list.
		 *\return		The iterator
		 *\~french
		 *\brief		Récupère un itérateur sur la fin de la liste de shaders.
		 *\return		L4itérateur
		 */
		inline ShaderProgramPtrArray::iterator end()
		{
			return m_arrayPrograms.end();
		}
		/**
		 *\~english
		 *\brief		Retrieves an iterator to the end of the shaders list.
		 *\return		The iterator
		 *\~french
		 *\brief		Récupère un itérateur sur la fin de la liste de shaders.
		 *\return		L4itérateur
		 */
		inline ShaderProgramPtrArray::const_iterator end()const
		{
			return m_arrayPrograms.end();
		}

	private:
		C3D_API ShaderProgramSPtr doAddProgram( ShaderProgramSPtr program, bool initialise );
		C3D_API ShaderProgramSPtr doCreateAutomaticProgram( RenderPass const & renderPass
			, PipelineFlags const & flags )const;
		C3D_API ShaderProgramSPtr doAddAutomaticProgram( ShaderProgramSPtr program
			, PipelineFlags const & flags );
		C3D_API ShaderProgramSPtr doCreateBillboardProgram( RenderPass const & renderPass
			, PipelineFlags const & flags )const;
		C3D_API ShaderProgramSPtr doAddBillboardProgram( ShaderProgramSPtr program
			, PipelineFlags const & flags );

	private:
		using ShaderProgramMap = std::map< PipelineFlags, ShaderProgramSPtr >;
		mutable std::recursive_mutex m_mutex;
		//!\~english	The loaded shader programs.
		//!\~french		Les programmes chargés.
		ShaderProgramPtrArray m_arrayPrograms;
		//!\~english	Automatically generated shader programs, sorted by texture flags.
		//!\~french		Programmes auto-générés, triés par flags de texture.
		ShaderProgramMap m_mapAutogenerated;
		//!\~english	Billboards shader programs, sorted by texture flags.
		//!\~french		Programmes shader pour billboards, triés par flags de texture.
		ShaderProgramMap m_mapBillboards;
	};
	/**
	 *\~english
	 *\brief		Creates a ashes::ShaderProgram cache.
	 *\param[in]	engine	The engine.
	 *\~french
	 *\brief		Crée un cache de ashes::ShaderProgram.
	 *\param[in]	engine	Le moteur.
	 */
	inline std::unique_ptr< ShaderProgramCache >
	makeCache( Engine & engine )
	{
		return std::make_unique< ShaderProgramCache >( engine );
	}
}

#endif