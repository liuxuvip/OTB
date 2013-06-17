/*=========================================================================

  Program:   Monteverdi2
  Language:  C++


  Copyright (c) Centre National d'Etudes Spatiales. All rights reserved.
  See Copyright.txt for details.

  Monteverdi2 is distributed under the CeCILL licence version 2. See
  Licence_CeCILL_V2-en.txt or
  http://www.cecill.info/licences/Licence_CeCILL_V2-en.txt for more details.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#ifndef __mvdImageProperties_h
#define __mvdImageProperties_h

//
// Configuration include.
//// Included at first position before any other ones.
#include "ConfigureMonteverdi2.h"


/*****************************************************************************/
/* INCLUDE SECTION                                                           */

//
// Qt includes (sorted by alphabetic order)
//// Must be included before system/custom includes.
#include <QtCore>

//
// System includes (sorted by alphabetic order)

//
// ITK includes (sorted by alphabetic order)

//
// OTB includes (sorted by alphabetic order)

//
// Monteverdi includes (sorted by alphabetic order)
#include "mvdTypes.h"


/*****************************************************************************/
/* PRE-DECLARATION SECTION                                                   */

//
// External classes pre-declaration.
namespace
{
}

namespace mvd
{
//
// Internal classes pre-declaration.


/*****************************************************************************/
/* CLASS DEFINITION SECTION                                                  */

/**
 * \class ImageProperties
 *
 * \brief WIP.
 */
class Monteverdi2_EXPORT ImageProperties :
    public QObject
{

  /*-[ QOBJECT SECTION ]-----------------------------------------------------*/

  Q_OBJECT;

  Q_PROPERTY( bool isNoDataEnabled
	      READ IsNoDataEnabled
	      WRITE SetNoDataEnabled );

  Q_PROPERTY( ComponentType NoData
	      READ GetNoData
	      WRITE SetNoData );

  /*-[ PUBLIC SECTION ]------------------------------------------------------*/

//
// Public methods.
public:

  /** \brief Constructor. */
  ImageProperties( QObject* parent =NULL );

  /** \brief Destructor. */
  virtual ~ImageProperties();

  /*
   */
  bool IsNoDataEnabled() const;

  /**
   */
  inline void SetNoDataEnabled( bool enabled );

  /**
   */
  void SetNoData( ComponentType value = ComponentType( 0 ) );

  /**
   */
  inline ComponentType GetNoData() const;

  /*-[ PUBLIC SLOTS SECTION ]------------------------------------------------*/

//
// Public SLOTS.
public slots:

  /*-[ SIGNALS SECTION ]-----------------------------------------------------*/

//
// Signals.
signals:

  /*-[ PROTECTED SECTION ]---------------------------------------------------*/

//
// Protected methods.
protected:

//
// Protected attributes.
protected:

  /*-[ PRIVATE SECTION ]-----------------------------------------------------*/

//
// Private methods.
private:


//
// Private attributes.
private:

  //
  // Group bitfield bool flags together.
  struct Flags
  {
    Flags() :
      m_NoData( false )
    {
    }

  public:
    bool m_NoData : 1;
  };

  /**
   */
  Flags m_Flags;

  /**
   */
  ComponentType m_NoData;

  /*-[ PRIVATE SLOTS SECTION ]-----------------------------------------------*/

//
// Slots.
private slots:
};

} // end namespace 'mvd'.

/*****************************************************************************/
/* INLINE SECTION                                                            */

//
// Qt includes (sorted by alphabetic order)
//// Must be included before system/custom includes.

//
// System includes (sorted by alphabetic order)

//
// ITK includes (sorted by alphabetic order)

//
// OTB includes (sorted by alphabetic order)

//
// Monteverdi includes (sorted by alphabetic order)

namespace mvd
{

/*****************************************************************************/
inline
void
ImageProperties
::SetNoDataEnabled( bool enabled )
{
  m_Flags.m_NoData = enabled;
}

/*****************************************************************************/
inline
void
ImageProperties
::SetNoData( ComponentType value )
{
  m_NoData = value;
}

/*****************************************************************************/
inline
bool
ImageProperties
::IsNoDataEnabled() const
{
  return m_Flags.m_NoData;
}

/*****************************************************************************/
inline
ComponentType
ImageProperties
::GetNoData() const
{
  return m_NoData;
}

} // end namespace 'mvd'

#endif // __mvdImageProperties_h
