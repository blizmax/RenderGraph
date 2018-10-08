/*
See LICENSE file in root folder
*/
#ifndef ___Utils_PARSER_PARAMETER_TYPE_EXCEPTION_H___
#define ___Utils_PARSER_PARAMETER_TYPE_EXCEPTION_H___

#include "Utils/Exception/Exception.hpp"

#include "ParserParameterHelpers.hpp"

namespace utils
{
	/*!
	\author 	Sylvain DOREMUS
	\date 		26/03/2013
	\version	0.7.0
	\~english
	\brief		Exception thrown when the user tries to retrieve a parameter of a wrong type
	\~french
	\brief		Exception lancée lorsque l'utilisateur se trompe de type de paramètre
	*/
	template< ParameterType GivenType >
	class ParserParameterTypeException
		: public utils::Exception
	{
	public:
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	p_expectedType	The real parameter type.
		 *\~french
		 *\brief		Constructeur
		 *\param[in]	p_expectedType	Le type réel du paramètre.
		 */
		inline explicit ParserParameterTypeException( ParameterType p_expectedType )
			: utils::Exception( "", "", "", 0 )
		{
			m_description = "Wrong parameter type in parser: user gave " + string::stringCast< xchar >( ParserParameterHelper< GivenType >::StringType ) + " while parameter base type is " + string::stringCast< xchar >( getTypeName( p_expectedType ) );
		}
	};
}

#endif