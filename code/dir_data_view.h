#pragma once
#include "fae_lib.h"
#include "new_filesystem.h"


class dir_tree_model_node
{
public:
	dir_tree_model_node(dir_tree_model_node* parent, wxString path, int depth = 0)
	{
		this->parent = parent;
		this->path = path;
		this->type = get_node_type(path);
		this->size = get_size(path);

		// @todo: this is a hack @hack
		if (depth > 0 && type == nt_normal_directory)
		{
			wxDir dir(path);
			assert(dir.IsOpened());
			wxString filename;
			bool has_children = dir.GetFirst(&filename);
			while(has_children)
			{
				children.push_back(new dir_tree_model_node(this, dir.GetNameWithSep() + filename, depth - 1));
				has_children = dir.GetNext(&filename);
			}
			
		}
	}

	~dir_tree_model_node()
	{
		for (auto child : children)
		{
			delete child;
		}
	}

	wxString path;
	Node_Type type;
	wxULongLong size;

	dir_tree_model_node* parent;
	vector<dir_tree_model_node*> children;
};


class dir_data_view_model : public wxDataViewModel
{
public:

	dir_data_view_model(wxString starting_path)
	{
		root = new dir_tree_model_node(null, starting_path, 1);
	}

	~dir_data_view_model()
	{
		delete root;
	}

	virtual bool HasContainerColumns(const wxDataViewItem& item) const override
	{
		return true;
	}

	virtual unsigned int GetColumnCount() const override
	{
		return 3;
	}

	// return type as reported by wxVariant
	virtual wxString GetColumnType(unsigned int col) const override
	{
		return wxT("string");
	}

	// get value into a wxVariant
	void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override
	{
		assert(item.IsOk());
		dir_tree_model_node* node = (dir_tree_model_node*)item.GetID();

		if (col == 0)
		{
			variant = node->path;
		}
		else if (col == 1)
		{
			variant = name(node->type);
		}
		else if (col == 2)
		{
			variant = wxFileName::GetHumanReadableSize(node->size);
		}
		else
		{
			con << "Error: dir_tree_model_node does not have a column " << col << endl;
		}
	}


	// usually ValueChanged() should be called after changing the value in the
	// model to update the control, ChangeValue() does it on its own while
	// SetValue() does not -- so while you will override SetValue(), you should
	// be usually calling ChangeValue()
	bool SetValue(const wxVariant &variant, const wxDataViewItem &item, unsigned int col)
	{
		//con << "SetValue() col: " << col << endl;
		assert(item.IsOk());
		dir_tree_model_node* node = (dir_tree_model_node*)item.GetID();
		if (col == 0)
		{
			node->path = variant.GetString();
			return true;
		}
		else if (col == 1)
		{
			node->type = (Node_Type)variant.GetLong();
			return true;
		}
		else if (col == 2)
		{
			node->size = variant.GetULongLong();
			return true;
		}
		else
		{
			con << "Error: dir_tree_model_node does not have a column " << col << endl;
		}
		return false;
	}

	// define hierarchy
	wxDataViewItem GetParent(const wxDataViewItem &item) const override
	{
		if (!item.IsOk())
			return wxDataViewItem(0);

		dir_tree_model_node* node = (dir_tree_model_node*)item.GetID();
		return wxDataViewItem((void*)node->parent);
	}

	bool IsContainer(const wxDataViewItem &item) const override
	{
		dir_tree_model_node* node = (dir_tree_model_node*)item.GetID();
		if (!node)
			return true;
		return node->type == nt_normal_directory;
	}

	unsigned int GetChildren(const wxDataViewItem &item, wxDataViewItemArray &children) const override
	{
		dir_tree_model_node* node = (dir_tree_model_node*)item.GetID();
		if (!node)
		{
			children.Add(wxDataViewItem((void*)root));
			return 1;
		}
		
		size_t num_children = node->children.size();
		for (size_t i = 0; i < num_children; i++)
		{
			dir_tree_model_node* child = node->children.at(i);
			children.Add(wxDataViewItem((void*)child));
		}
		return num_children;
	}

	//// Helper function used by the default Compare() implementation to compare
	//// values of types it is not aware about. Can be overridden in the derived
	//// classes that use columns of custom types.
	//virtual int DoCompareValues(const wxVariant& WXUNUSED(value1), const wxVariant& WXUNUSED(value2)) const
	//{
	//	return 0;
	//}

	dir_tree_model_node* root;
};
