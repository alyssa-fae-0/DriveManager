#pragma once
#include "fae_lib.h"
#include "console.h"

struct Queue_display_adaptor : public wxAddRemoveAdaptor
{
	wxListBox* list_box;

	Queue_display_adaptor(wxListBox* list_box)
	{
		this->list_box = list_box;
	}

	wxWindow* GetItemsCtrl() const wxOVERRIDE
	{
		return list_box;
	}

	bool CanAdd() const wxOVERRIDE
	{
		return true;
	}

	bool CanRemove() const wxOVERRIDE
	{
		return list_box->GetSelection() != wxNOT_FOUND;
	}

	void OnAdd() wxOVERRIDE
	{
		// A real program would use a wxDataViewCtrl or wxListCtrl and
		// allow editing the newly edited item in place, here we just use a
		// hardcoded item value instead.
		static int s_item = 0;
		list_box->Append(wxString::Format("new item #%d", ++s_item));
	}

	void OnRemove() wxOVERRIDE
	{
		// Notice that we don't need to check if we have a valid selection,
		// we can be only called if CanRemove(), which already checks for
		// this, had returned true.
		const unsigned pos = list_box->GetSelection();

		list_box->Delete(pos);
		list_box->SetSelection(pos == list_box->GetCount() ? pos - 1 : pos);
	}
	
};

struct Queue_display : public wxAddRemoveCtrl
{
	Queue_display(wxWindow* parent, int id)
		: wxAddRemoveCtrl(parent)
	{
		auto list_box = new wxListBox(parent, id);

		for (int i = 0; i < 7; i++)
		{
			wxString text;
			text << "Item " << i;
			list_box->Insert(text, i);
		}
		auto adaptor = new Queue_display_adaptor(list_box);
		this->SetAdaptor(adaptor);
		auto children = GetChildren();
		for (size_t i = 0; i < children.size(); i++)
		{
			auto child = children[i];
			con << "List child: " << child->GetName() << endl;
		}
		//ctrl->RemoveChild();
	}
};