#include "GuiCommon/PrecompiledHeader.h"

#include "GuiCommon/MaterialsFrame.h"
#include "GuiCommon/MaterialsListView.h"
#include "GuiCommon/MaterialPanel.h"

using namespace Castor3D;
using namespace GuiCommon;

MaterialsFrame :: MaterialsFrame( MaterialManager * p_pManager, wxWindow * parent, const wxString & title,
									  const wxPoint & pos, const wxSize & size)
	:	wxFrame( parent, wxID_ANY, title, pos, size, wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN | wxFRAME_FLOAT_ON_PARENT)
	,	m_listWidth( 120)
	,	m_pManager( p_pManager)
{
	wxSize l_size = GetClientSize();
	m_materialsList = new MaterialsListView( m_pManager, this, eMaterialsList, wxPoint( l_size.x - m_listWidth, 0), wxSize( m_listWidth, l_size.y));
	m_materialPanel = new MaterialPanel( m_pManager, this, wxPoint( 0, 25), wxSize( l_size.x - m_listWidth, l_size.y - 25));
	m_materialsList->Show();
}

MaterialsFrame :: ~MaterialsFrame()
{
}

void MaterialsFrame :: CreateMaterialPanel( const String & p_materialName)
{
	m_selectedMaterial = m_pManager->GetElementByName( p_materialName);
	m_materialPanel->CreateMaterialPanel( p_materialName);
}

BEGIN_EVENT_TABLE( MaterialsFrame, wxFrame)
	EVT_SHOW(									MaterialsFrame::_onShow)
	EVT_LIST_ITEM_SELECTED(		eMaterialsList, MaterialsFrame::_onSelected)
	EVT_LIST_ITEM_DESELECTED(	eMaterialsList, MaterialsFrame::_onDeselected)
END_EVENT_TABLE()

void MaterialsFrame :: _onShow( wxShowEvent & event)
{
	if (event.GetShow())
	{
		m_materialsList->CreateList();
		CreateMaterialPanel( C3DEmptyString);
	}
}

void MaterialsFrame :: _onSelected( wxListEvent& event)
{
	CreateMaterialPanel( event.GetText().c_str());
}

void MaterialsFrame :: _onDeselected( wxListEvent& event)
{
	if (m_selectedMaterial != NULL)
	{
		m_materialsList->CreateList();
		m_selectedMaterial.reset();
	}

	CreateMaterialPanel( C3DEmptyString);
}