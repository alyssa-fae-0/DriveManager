#pragma once
#include "fae_lib.h"
#include "new_filesystem.h"

class fs_model;
extern wxObjectDataPtr<fs_model> Model;

class fs_node
{
public:

	//u32 uid;
	wxString name;
	Node_Type type;
	wxULongLong size;

	fs_node* parent;
	vector<fs_node*> children;
	wxCriticalSection children_cs;

	//wxString path; // on the chopping block


	fs_node(fs_node* parent, const wxString& path)
	{
		//this->uid = uid;
		this->parent = parent;
		this->name = get_name(path);

		this->type = get_node_type(path);
		// if (type == Node_Type::nt_normal_directory) con << "calculating size for " << path << endl;
		if(this->type != Node_Type::normal_directory)
			this->size = get_size(path);
		//if (type == Node_Type::nt_normal_directory) con << "size calculated" << endl;
	}

	~fs_node()
	{
		for (auto child : children)
		{
			delete child;
		}
	}

	bool is_root()
	{
		return this->parent == null;
	}

	void build_path(wxString& path)
	{
		if (!is_root())
		{
			parent->build_path(path);
		}
		path.append(name);
		path.append(wxFileName::GetPathSeparator());
	}

	wxString get_path()
	{
		wxString path;
		if (parent != null)
		{
			parent->build_path(path);
		}
		path.append(name);
		con << "Built path: " << path << endl;
		return path;
	}

	// @todo: should this be folded into the thread traversing?
	void update_children()
	{
		const wxString path = get_path();
		wxDir dir(path);
		assert(dir.IsOpened());

		wxString base = path;

		// add a separator if it doesn't end with one already
		if (!wxFileName::IsPathSeparator(base.Last()))
			base.append(wxFileName::GetPathSeparator());

		// gather all the filenames into one collection,
		//   to minimize the time we lock the children vector
		vector<wxString> files;
		wxString filename;
		bool has_children = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
		while (has_children)
		{
			files.push_back(base + filename);
			has_children = dir.GetNext(&filename);
		}

		// get all the file names at once, to minimize time that we lock the children
		{
			wxCriticalSectionLocker enter(this->children_cs);
			for (auto file : files)
			{
				children.push_back(new fs_node(this, file));
			}
		}
		

	}
};


class fs_thread : public wxThread
{
public:

	fs_node* node;
	vector<fs_thread*>* threads;
	wxCriticalSection* threads_cs;

	fs_thread(fs_node* node, vector<fs_thread*>* threads, wxCriticalSection* threads_cs)
		: wxThread(wxTHREAD_DETACHED)
	{
		this->node = node;
		this->threads = threads;
		this->threads_cs = threads_cs;
	}

	~fs_thread()
	{
		wxCriticalSectionLocker enter(*threads_cs);

		// @bug? We may be missing the final element, depending on how .end() works
		for (auto iter = threads->begin(); iter < threads->end(); iter++)
		{
			if (*iter == this)
			{
				*iter = null;
			}
		}
	}

	wxThread::ExitCode Entry() override
	{
		while (!TestDestroy())
		{
			//node->size = get_size(node->path);
			//wxQueueEvent(window, new wxDataViewEvent(wxevt_dataview, null, wxDataViewItem(node)));
		}

		// signal the event handler that this thread is going to be destroyed
		// NOTE: here we assume that using the m_pHandler pointer is safe,
		//       (in this case this is assured by the MyFrame destructor)
		//wxQueueEvent(window, new wxThreadEvent(wxEVT_COMMAND_MYTHREAD_COMPLETED));

		return (wxThread::ExitCode)0;     // success
	}


};


class fs_model : public wxDataViewModel
{
public:

	// @todo: make this an array or vector
	vector<fs_node*> roots;

	// @note:	this is the centralized uid source; all uids for this model come from here;
	//			uids can't be returned, so if we end up implementing a system for getting rid
	//			of nodes, we shouldnt delete the nodes (or we'll risk running out of uids);
	//			maybe we should use a "freed nodes" list and have freed nodes added to that
	u32 cur_uid;
	wxCriticalSection cur_uid_cs;

	// this is the source of all of the threads in the app; it uses a critical section to synchronize
	// additions and removals of threads (I think that's needed so that we don't run into a situation
	// where we have e.g. two threads trying to reclaim an empty slot at the same time?)
	vector<fs_thread*> threads;
	wxCriticalSection threads_cs;
	const int max_threads = 8; // @todo: come up with a programatic way to derive this number

	wxWindow* parent_window;

	u32 get_new_uid() 
	{
		u32 get_id = 0;
		// make the time that we hold on to the cur_uid as short as possible
		{
			wxCriticalSectionLocker enter(cur_uid_cs);
			get_id = cur_uid++;
		}
		return get_id;
	}

	void spawn_thread(fs_node* node)
	{
		fs_thread* thread = null;
		{
			wxCriticalSectionLocker enter(threads_cs);
			thread = new fs_thread(node, &threads, &threads_cs);
			threads.push_back(thread);
		}
		if (thread->Run() != wxTHREAD_NO_ERROR)
		{
			con << "ERROR: Failed to create thread" << endl;
			delete thread;

			wxCriticalSectionLocker enter(threads_cs);
			for (auto iter = threads.begin(); iter <= threads.end(); iter++)
			{
				if (*iter == thread)
				{
					*iter = null;
				}
			}
		}
	}

	void kill_threads()
	{
		{
			wxCriticalSectionLocker enter(threads_cs);
			for (auto thread : threads)
			{
				if (thread != null)
					if (thread->Delete() != wxTHREAD_NO_ERROR)
						con << "Failed to delete thread" << endl;

			}
		}

		while(true)
		{
			{
				wxCriticalSectionLocker enter(threads_cs);
				bool all_dead = true;
				for (auto thread : threads)
				{
					if (thread != null)
					{
						all_dead = false;
					}
				}
				if (all_dead)
					break;
			}

			// wait for the threads to finish
			wxThread::This()->Sleep(1);
		}
	}



	fs_model(wxString starting_path)
	{
		// get all drives
		vector<wxString> drives;

		// Get all connected drives
		DWORD drive_mask = GetLogicalDrives();
		for (DWORD i = 0; i < 26; i++)
		{
			char drive_letter = (char)(i + 'A');
			if ((drive_mask >> i) & 1)
				drives.push_back(wxString(drive_letter) + ":");
				//else
			//	printf("Drive %c: Not Detected\n", drive_letter);
		}

		// create 
		for (auto drive : drives)
		{
			roots.push_back(new fs_node(null, drive));
		}






		vector<wxString> paths;
		int length = starting_path.length();
		int i = 0;
		auto iter = starting_path.begin();
		while ( i < length)
		{
			// found path separator
			if (wxFileName::IsPathSeparator(*iter))
			{
				
				// don't include the path_separator 
				if(i == (length - 1))
					paths.push_back(starting_path.substr(0, i));
				else
					paths.push_back(starting_path.substr(0, i));

			}

			iter++;
			i++;
		}
	}

	~fs_model()
	{
		for(auto root : roots)
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
		fs_node* node = (fs_node*)item.GetID();

		if (col == 0)
		{
			variant = get_name(node->name);
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
			con << "Error: fs_node does not have a column " << col << endl;
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
		fs_node* node = (fs_node*)item.GetID();
		if (col == 0)
		{
			node->name = variant.GetString();
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
			con << "Error: fs_node does not have a column " << col << endl;
		}
		return false;
	}

	// define hierarchy
	wxDataViewItem GetParent(const wxDataViewItem &item) const override
	{
		if (!item.IsOk())
			return wxDataViewItem(0);

		fs_node* node = (fs_node*)item.GetID();
		return wxDataViewItem((void*)node->parent);
	}

	bool IsContainer(const wxDataViewItem &item) const override
	{
		fs_node* node = (fs_node*)item.GetID();
		if (!node)
			return true;

		// @todo:	launch a thread here to traverse the directory; 
		//			return false in the meantime
		//			and queue an event once the thread has finished, indicating that the app needs to
		//				recheck the values of this item (which should trigger this function again)
		return node->type == Node_Type::normal_directory;
	}

	unsigned int GetChildren(const wxDataViewItem &item, wxDataViewItemArray &children) const override
	{
		fs_node* node = (fs_node*)item.GetID();
		if (!node)
		{
			for(auto root : roots)
				children.Add(wxDataViewItem((void*)root));
			return roots.size();
		}
		
		size_t num_children = node->children.size();
		for (size_t i = 0; i < num_children; i++)
		{
			fs_node* child = node->children.at(i);
			children.Add(wxDataViewItem((void*)child));
		}
		return num_children;
	}


	virtual int Compare(const wxDataViewItem &item1, const wxDataViewItem &item2, unsigned int column, bool ascending) const override
	{
		assert(item1.IsOk() && item2.IsOk());
		fs_node* node1 = (fs_node*)item1.GetID();
		fs_node* node2 = (fs_node*)item2.GetID();

		if(column == 0)
		{
			wxString name1 = node1->name.Upper();
			wxString name2 = node2->name.Upper();
			if (ascending)
				return name1.compare(name2);
			else
				return name2.compare(name1);
		}
		return wxDataViewModel::Compare(item1, item2, column, ascending);
		//
		//switch (column)
		//{
		//case 0: // name
		//{
		//	wxString name1 = node1->name.Upper();
		//	wxString name2 = node2->name.Upper();
		//	if (ascending)
		//		return name1.compare(name2);
		//	else
		//		return name2.compare(name1);
		//}

		//case 1: // node type
		//{
		//	wxString type1 = name(node1->type); 
		//	type1.Upper();
		//	wxString type2 = name(node2->type); 
		//	type2.Upper();
		//	if (ascending)
		//		return type1.compare(type2);
		//	else
		//		return type2.compare(type1);
		//}

		//case 2: // size
		//{
		//	auto size1 = node1->size;
		//	auto size2 = node2->size;
		//	if (ascending)
		//	{
		//		if (size1 < size2)
		//			return -1;
		//		else if (size1 == size2)
		//			return 0;
		//		else
		//			return 1;
		//	}
		//	else
		//	{
		//		if (size1 < size2)
		//			return 1;
		//		else if (size1 == size2)
		//			return 0;
		//		else
		//			return -1;
		//	}
		//}
		//}

		//assert(false); // should never get here
		//return 0;
	}

	//// Helper function used by the default Compare() implementation to compare
	//// values of types it is not aware about. Can be overridden in the derived
	//// classes that use columns of custom types.
	//virtual int DoCompareValues(const wxVariant& WXUNUSED(value1), const wxVariant& WXUNUSED(value2)) const
	//{
	//	return 0;
	//}

};