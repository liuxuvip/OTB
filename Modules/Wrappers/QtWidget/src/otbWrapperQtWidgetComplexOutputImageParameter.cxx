/*
 * Copyright (C) 2005-2019 Centre National d'Etudes Spatiales (CNES)
 *
 * This file is part of Orfeo Toolbox
 *
 *     https://www.orfeo-toolbox.org/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "otbWrapperQtWidgetComplexOutputImageParameter.h"


#include <otbQtAdapters.h>
#include <otbWrapperTypes.h>

namespace otb
{
namespace Wrapper
{

QtWidgetComplexOutputImageParameter::QtWidgetComplexOutputImageParameter(ComplexOutputImageParameter* param, QtWidgetModel* m, QWidget * parent)
: QtWidgetParameterBase(param, m, parent),
  m_OutputImageParam(param)
{
}

QtWidgetComplexOutputImageParameter::~QtWidgetComplexOutputImageParameter()
{
}

const QLineEdit*
QtWidgetComplexOutputImageParameter
::GetInput() const
{
  return m_Input;
}

QLineEdit*
QtWidgetComplexOutputImageParameter
::GetInput()
{
  return m_Input;
}

void QtWidgetComplexOutputImageParameter::DoUpdateGUI()
{
  // Update the lineEdit
  QString text(
    QFile::decodeName( m_OutputImageParam->GetFileName() )
  );

  if (text != m_Input->text())
    m_Input->setText(text);
}

void QtWidgetComplexOutputImageParameter::DoCreateWidget()
{
  // Set up input text edit
  m_HLayout = new QHBoxLayout;
  m_HLayout->setSpacing(0);
  m_HLayout->setContentsMargins(0, 0, 0, 0);
  m_Input = new QLineEdit(this);
  m_Input->setToolTip(
    QString::fromStdString( m_OutputImageParam->GetDescription() )
  );
  connect( m_Input, &QLineEdit::textChanged, this, &QtWidgetComplexOutputImageParameter::SetFileName );
  connect( m_Input, &QLineEdit::textChanged, GetModel(), &QtWidgetModel::NotifyUpdate );
  m_HLayout->addWidget(m_Input);

  // Set the Output PixelType choice Combobox
  m_ComboBox = new QComboBox(this);
  m_ComboBox->setToolTip("Complex Output Pixel Type");
  m_ComboBox->addItem( "cint16");
  m_ComboBox->addItem( "cint32");
  m_ComboBox->addItem( "cfloat");
  m_ComboBox->addItem( "cdouble");
  m_ComboBox->setCurrentIndex(m_OutputImageParam->GetComplexPixelType());
  connect( m_ComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &QtWidgetComplexOutputImageParameter::SetPixelType );
  connect( m_ComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), GetModel(), &QtWidgetModel::NotifyUpdate );
  m_HLayout->addWidget(m_ComboBox);

  // Set up input text edit
  m_Button = new QPushButton(this);
  m_Button->setText("...");
  m_Button->setToolTip("Select output filename...");
  m_Button->setMaximumWidth(m_Button->width());
  connect( m_Button, &QPushButton::clicked, this, &QtWidgetComplexOutputImageParameter::SelectFile );
  m_HLayout->addWidget(m_Button);

  this->setLayout(m_HLayout);
}


void
QtWidgetComplexOutputImageParameter
::SelectFile()
{
  assert( m_Input!=NULL );

  QString filename(
    otb::GetSaveFilename(
      this,
      QString(),
      m_Input->text(),
      tr( "Raster files (*)" ),
      NULL
    )
  );

  if( filename.isEmpty() )
    return;

  SetFileName( filename );

  m_Input->setText( filename  );
}


void QtWidgetComplexOutputImageParameter::SetFileName(const QString& value)
{
  // save value
  m_FileName = QFile::encodeName( value ).constData();

  m_OutputImageParam->SetFileName(m_FileName);

  // notify of value change
  QString key( m_OutputImageParam->GetKey() );
  emit ParameterChanged(key);
}

void QtWidgetComplexOutputImageParameter::SetPixelType(int  pixelType)
{
  m_OutputImageParam->SetComplexPixelType(static_cast< ComplexImagePixelType >(pixelType));
  m_ComplexPixelType = pixelType;
}

}
}
