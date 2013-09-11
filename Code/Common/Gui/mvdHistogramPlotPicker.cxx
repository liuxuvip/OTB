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
#include "Gui/mvdHistogramPlotPicker.h"


/*****************************************************************************/
/* INCLUDE SECTION                                                           */

//
// Qt includes (sorted by alphabetic order)
//// Must be included before system/custom includes.

//
// Qwt includes.
#include <qwt_plot_curve.h>
#include <qwt_data.h>
#include <qwt_painter.h>
#include <qwt_plot.h>
#include <qwt_text.h>

//
// System includes (sorted by alphabetic order)
#include <cassert>

//
// ITK includes (sorted by alphabetic order)

//
// OTB includes (sorted by alphabetic order)

//
// Monteverdi includes (sorted by alphabetic order)
#include "Gui/mvdHistogramWidget.h"

namespace mvd
{

/*
  TRANSLATOR mvd::HistogramPlotPicker

  Necessary for lupdate to be aware of C++ namespaces.

  Context comment for translator.
*/


/*****************************************************************************/
/* CONSTANTS                                                                 */


/*****************************************************************************/
/* STATIC IMPLEMENTATION SECTION                                             */


/*****************************************************************************/
/* CLASS IMPLEMENTATION SECTION                                              */

/*******************************************************************************/
HistogramPlotPicker
::HistogramPlotPicker( const PlotCurveVector& curves, QwtPlotCanvas* canvas ) :
  QwtPlotPicker( canvas ),
  m_PlotCurves( curves ),
  m_RubberBandPens(),
  m_IsGrayscaleActivated( false )
{
  assert( m_PlotCurves.size()==HistogramPlotPicker::CURVE_COUNT );

  setSelectionFlags( QwtPicker::PointSelection );
  setRubberBand( QwtPicker::UserRubberBand );
}

/*******************************************************************************/
HistogramPlotPicker
::HistogramPlotPicker( const PlotCurveVector& curves,
		       int xAxis,
		       int yAxis,
		       QwtPlotCanvas* canvas ) :
  QwtPlotPicker( xAxis, yAxis, canvas ),
  m_PlotCurves( curves ),
  m_RubberBandPens(),
  m_IsGrayscaleActivated( false )

{
  assert( m_PlotCurves.size()==HistogramPlotPicker::CURVE_COUNT );

  setSelectionFlags( QwtPicker::PointSelection );
  setRubberBand( QwtPicker::UserRubberBand );
}

/*******************************************************************************/
HistogramPlotPicker
::HistogramPlotPicker( const PlotCurveVector& curves,
		       int xAxis,
		       int yAxis,
		       int selectionFlags,
		       DisplayMode trackerMode,
		       QwtPlotCanvas* canvas ) :
  QwtPlotPicker(
    xAxis,
    yAxis,
    QwtPicker::PointSelection,
    QwtPicker::UserRubberBand,
    trackerMode,
    canvas ),
  m_PlotCurves( curves ),
  m_RubberBandPens(),
  m_IsGrayscaleActivated( false )
{
  assert( m_PlotCurves.size()==HistogramPlotPicker::CURVE_COUNT );
}

/*******************************************************************************/
HistogramPlotPicker
::~HistogramPlotPicker()
{
}

/*******************************************************************************/
void
HistogramPlotPicker
::SetRubberBandPen( RgbwChannel channel, const QPen& pen )
{
  CountType begin = 0;
  CountType end = 0;

  if( !RgbwBounds( begin, end, channel ) )
    return;

  for( CountType i=begin; i<end; ++i )
    {
    qDebug() << RGBW_CHANNEL_NAMES[ i ] << pen;

    m_RubberBandPens[ i ] = pen;
    }
}

/*******************************************************************************/
void
HistogramPlotPicker
::SetGrayscaleActivated( bool activated )
{
  m_IsGrayscaleActivated = activated;
}

/*******************************************************************************/
void
HistogramPlotPicker
::drawRubberBand( QPainter* painter ) const
{
  assert( painter!=NULL );

  if( rubberBand()!=QwtPicker::UserRubberBand )
    {
    QwtPlotPicker::drawRubberBand( painter );
    return;
    }

  if( !isActive() ||
      rubberBand()==QwtPicker::NoRubberBand ||
      rubberBandPen().style()==Qt::NoPen )
    return;

  const QwtPolygon& pa = selection();

  if( rubberBand()==QwtPicker::UserRubberBand &&
      ( selectionFlags() & PointSelection ) &&
      selection().count() >= 1 )
    {   
    const QRect& rect = pickRect();
    const QPoint& pos = pa[ 0 ];
   
    QwtPainter::drawLine(
      painter,
      pos.x(), rect.bottom(),
      pos.x(), rect.top()
    );

    CountType begin = 0;
    CountType end = 0;

    if( !RgbwBounds( begin,
		     end,
		     m_IsGrayscaleActivated
		     ? RGBW_CHANNEL_WHITE
		     : RGBW_CHANNEL_RGB ) )
      return;

    QwtDoublePoint p( invTransform( pos ) );

    for( CountType i=begin; i<end; ++i )
      {
      // RgbwChannel channel = static_cast< RgbwChannel >( i );

      p.setY( Find( m_PlotCurves[ i ], p.x() ) );
      QPoint pos2( transform( p ) );

      /*
      QPen pen( m_RubberBandPens[ i ] );
      painter->setPen( rubberBandPen() );
      */

      QwtPainter::drawLine(
	painter,
	rect.left(), pos2.y(),
	rect.right(), pos2.y()
      );

      // painter->setPen( pen );
      }
    }
}

/*******************************************************************************/
QwtText
HistogramPlotPicker
::trackerText( const QwtDoublePoint& point ) const
{
  // qDebug() << this << "::trackerText(" << point << ")";

  if( rubberBand()!=QwtPicker::UserRubberBand )
    return QwtPlotPicker::trackerText( point );

  if( !isActive() )
    return QwtText(
      QString().sprintf(
      "%.4f, %.4f",
      point.x(), point.y()
      )
    );

  /*
  assert( plot()!=NULL );
  assert(
    plot()->parent()==qobject_cast< const HistogramWidget* >( plot()->parent() )
  );

  const HistogramWidget* widget =
    qobject_cast< const HistogramWidget* >( plot()->parent() );
  assert( widget!=NULL );
  */

  QwtText text;

  if( m_IsGrayscaleActivated )
    {
    double w0 = 0.0;
    double w1 = 0.0;
    double wf = 0.0;

    Find( m_PlotCurves[ RGBW_CHANNEL_WHITE ], point.x(), w0, w1, wf );

    text = QString().sprintf(
      // "[%.4f; %.4f[ %.0f",
      "%.4f\n%.4f/%.4f/%.0f",
      point.x(),
      w0, w1, wf
    );
    }
  else
    {
    double r0 = 0.0;
    double r1 = 0.0;
    double rf = 0.0;

    Find( m_PlotCurves[ RGBW_CHANNEL_RED ], point.x(), r0, r1, rf );

    double g0 = 0.0;
    double g1 = 0.0;
    double gf = 0.0;

    Find( m_PlotCurves[ RGBW_CHANNEL_GREEN ], point.x(), g0, g1, gf );

    double b0 = 0.0;
    double b1 = 0.0;
    double bf = 0.0;

    Find( m_PlotCurves[ RGBW_CHANNEL_BLUE ], point.x(), b0, b1, bf );

    text = QString().sprintf(
      /*
      "[%.4f; %.4f[ %.0f\n"
      "[%.4f; %.4f[ %.0f\n"
      "[%.4f; %.4f[ %.0f",
      */
      "%.4f\n"
      "%.4f/%.4f/%.0f\n"
      "%.4f/%.4f/%.0f\n"
      "%.4f/%.4f/%.0f",
      point.x(),
      r0, r1, rf,
      g0, g1, gf,
      b0, b1, bf
    );
    }

  return text;
}

/*******************************************************************************/
double
HistogramPlotPicker
::Find( const QwtPlotCurve* curve, double x ) const
{
  // qDebug() << this << "::Find(" << curve << ", " << x << ")";

  const QwtData& data = curve->data();

/*
#if HISTOGRAM_CURVE_TYPE==0
  assert( false && "Not yet implemented!" );

#elif HISTOGRAM_CURVE_TYPE==1
  assert( false && "Not yet implemented!" );

#elif HISTOGRAM_CURVE_TYPE==2
*/

  assert( data.size() % 4 == 0 );

  CountType steps = 0;

  if( data.size()==0 )
    return -1.0;

  CountType i0 = 0;
  CountType i1 = data.size() / 4 - 1;

  // assert( x>=data.x( 4 * i0 ) && x<=data.x( 4 * i1 + 3 ) );
  if( x<data.x( 4 * i0 ) || x>data.x( 4 * i1 + 3 ) )
    return -1.0;

  while( i0!=i1 )
    {
    assert( data.x( 4 * i0 )==data.x( 4 * i0 + 1 ) );
    assert( data.x( 4 * i0 + 2 )==data.x( 4 * i0 + 3 ) );
    assert( data.y( 4 * i0 + 1 )==data.y( 4 * i0 + 2 ) );
    assert( data.y( 4 * i0 )==data.y( 4 * i0 + 3 ) );

    assert( data.x( 4 * i1 )==data.x( 4 * i1 + 1 ) );
    assert( data.x( 4 * i1 + 2 )==data.x( 4 * i1 + 3 ) );
    assert( data.y( 4 * i1 + 1 )==data.y( 4 * i1 + 2 ) );
    assert( data.y( 4 * i1 )==data.y( 4 * i1 + 3 ) );

    CountType i = (i0 + i1 + 1) / 2;

#if 0
    qDebug()
      << i0 << " (" << data.x( 4*i0 ) << ", " << data.x( 4*i0+2 ) << ") "
      << i << " (" << data.x( 4*i ) << ", " << data.x( 4*i+2 ) << ") "
      << i1 << " (" << data.x( 4*i1 ) << ", " << data.x( 4*i1+2 ) << ")";
#endif

    if( x<data.x( 4 * i ) )
      i1 = i - 1;
    else
      i0 = i;

    ++ steps;
    }

  assert( x>=data.x( 4 * i0 ) && x<=data.x( 4 * i0 + 2 ) );
  assert( x>=data.x( 4 * i0 + 1 ) && x<=data.x( 4 * i0 + 3 ) );

#if 0
  qDebug()
    << steps << ":"
    << x << "in [" << data.x( 4 * i0 ) << "; " << data.x( 4 * i0 + 2 ) << "] ->"
    << data.y( 4 * i0 + 1 );
#endif

  return data.y( 4 * i0 + 1 );

/*
#else
  assert( false && "Unknown HISTOGRAM_CURVE_TYPE value" );

#endif
*/
}

/*******************************************************************************/
CountType
HistogramPlotPicker
::Find( const QwtPlotCurve* curve,
	double x,
	double& xmin,
	double& xmax,
	double& y ) const
{
  const QwtData& data = curve->data();

/*
#if HISTOGRAM_CURVE_TYPE==0
  assert( false && "Not yet implemented!" );

#elif HISTOGRAM_CURVE_TYPE==1
  assert( false && "Not yet implemented!" );

#elif HISTOGRAM_CURVE_TYPE==2
*/

  assert( data.size() % 4 == 0 );

  CountType steps = 0;

  if( data.size()==0 )
    return 0;

  CountType i0 = 0;
  CountType i1 = data.size() / 4 - 1;

  if( x<data.x( 4 * i0 ) || x>data.x( 4 * i1 + 3 ) )
    return 0;

  while( i0!=i1 )
    {
    assert( data.x( 4 * i0 )==data.x( 4 * i0 + 1 ) );
    assert( data.x( 4 * i0 + 2 )==data.x( 4 * i0 + 3 ) );
    assert( data.y( 4 * i0 + 1 )==data.y( 4 * i0 + 2 ) );
    assert( data.y( 4 * i0 )==data.y( 4 * i0 + 3 ) );

    assert( data.x( 4 * i1 )==data.x( 4 * i1 + 1 ) );
    assert( data.x( 4 * i1 + 2 )==data.x( 4 * i1 + 3 ) );
    assert( data.y( 4 * i1 + 1 )==data.y( 4 * i1 + 2 ) );
    assert( data.y( 4 * i1 )==data.y( 4 * i1 + 3 ) );

    CountType i = (i0 + i1 + 1) / 2;

#if 0
    qDebug()
      << i0 << " (" << data.x( 4*i0 ) << ", " << data.x( 4*i0+2 ) << ") "
      << i << " (" << data.x( 4*i ) << ", " << data.x( 4*i+2 ) << ") "
      << i1 << " (" << data.x( 4*i1 ) << ", " << data.x( 4*i1+2 ) << ")";
#endif

    if( x<data.x( 4 * i ) )
      i1 = i - 1;
    else
      i0 = i;

    ++ steps;
    }

  assert( x>=data.x( 4 * i0 ) && x<=data.x( 4 * i0 + 2 ) );
  assert( x>=data.x( 4 * i0 + 1 ) && x<=data.x( 4 * i0 + 3 ) );

#if 0
  qDebug()
    << steps << ":"
    << x << "in [" << data.x( 4 * i0 ) << "; " << data.x( 4 * i0 + 2 ) << "] ->"
    << data.y( 4 * i0 + 1 );
#endif

  xmin = data.x( 4 * i0 );
  xmax = data.x( 4 * i0 + 2 );
  y = data.y( 4 * i0 + 1 );

  return steps;

/*
#else
  assert( false && "Unknown HISTOGRAM_CURVE_TYPE value" );

#endif
*/
}

/*******************************************************************************/
/* SLOTS                                                                       */
/*******************************************************************************/

} // end namespace 'mvd'
