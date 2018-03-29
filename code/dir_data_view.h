#pragma once
#include "fae_lib.h"
#include "new_filesystem.h"
#include "app_events.h"

class fs_model;
extern wxObjectDataPtr<fs_model> Model;

class fs_node;
extern vector<fs_node*> nodes_to_calc_size;
extern wxCriticalSection nodes_to_calc_size_cs;

//struct nullable_size
//{
//	nullable_size()
//	{
//		is_null = true;
//		size = 0;
//	}
//
//	nullable_size(bool is_null, wxULongLong size)
//		: is_null(is_null), size(size) 
//	{ }
//
//	operator wxULongLong() 
//	{
//		return size;
//	}
//
//	bool is_null;
//	wxULongLong size;
//};

enum struct node_size_state : int
{
	waiting_processing,
	processing,
	unable_to_access_all_files,
	processing_complete
};

wxString size_state_name(node_size_state state)
{
	switch (state)
	{
	case node_size_state::waiting_processing:
		return "'Waiting for Processing'";
	case node_size_state::processing:
		return "'Processing'";
	case node_size_state::unable_to_access_all_files:
		return "'Unable to Process All Files'";
	case node_size_state::processing_complete:
		return "'Processing Complete'";
	}
	assert(false); // shouldn't get here
	return "ERROR: INVALID STATE";
}

struct node_size
{
	node_size_state state;
	wxULongLong val;
};

class fs_node
{
public:

	//u32 uid;
	wxString name;
	Node_Type type;

	node_size size;
	wxCriticalSection size_cs;

	fs_node* parent;
	vector<fs_node*> children;
	wxCriticalSection children_cs;

	//wxString path; // on the chopping block


	fs_node(fs_node* parent, const wxString& path)
	{
		//con << "Creating node: " << path << endl;
		//this->uid = uid;
		this->parent = parent;
		this->name = get_name(path);

		this->type = get_node_type(path);
		// if (type == Node_Type::nt_normal_directory) con << "calculating size for " << path << endl;
		
		
		if (this->type != Node_Type::normal_directory)
		{
			auto val = get_size(path);
			wxCriticalSectionLocker enter(size_cs);
			this->size.val = val;
			this->size.state = node_size_state::processing_complete;
		}
		else
		{
			{
				auto val = wxInvalidSize;
				wxCriticalSectionLocker(this->size_cs);
				size.val = val;
				size.state = node_size_state::waiting_processing;
			}
			{
				wxCriticalSectionLocker enter(nodes_to_calc_size_cs);
				nodes_to_calc_size.push_back(this);
			}
		}
		
		//if (type == Node_Type::nt_normal_directory) con << "size calculated" << endl;
	}

	~fs_node()
	{
		for (auto child : children)
		{
			delete child;
		}
	}

	wxString to_string()
	{
		wxString str;
		str << "Path: " << this->get_path();
		str << ",\n  Name: " << this->name;
		str << ",\n  Type: " << node_type_name(this->type);
		str << ",\n  Size State: ";
		{
			wxCriticalSectionLocker enter(this->size_cs);
			str << size_state_name(this->size.state);
			str << ",\n  Size: " << wxFileName::GetHumanReadableSize(this->size.val);
		}
		str << ",\n  Parent Name: ";
		if (this->parent == null)
			str << "NULL";
		else
			str << this->parent->name;
		str << ",\n  Children names: ";
		{
			wxCriticalSectionLocker enter(this->children_cs);
			if (this->children.size() == 0)
				str << "NULL";
			else
			{
				// actually has children
				for (auto child : children)
				{
					str << "\n    " << child->name;
				}
			}
		}
		return str;
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
		//con << "Built path: " << path << endl;
		return path;
	}

	// @todo: should this be folded into the thread traversing?
	// @todo @robustness: add better checks to only update children when we actually need to
	// @hack
	void update_children()
	{
		{
			wxCriticalSectionLocker enter(children_cs);
			if (children.size() != 0)
			{
				return;
			}
		}
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
				fs_node* node = new fs_node(this, file);
				//nodes_to_calc_size.push_back(node);
				children.push_back(node);
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
	wxWindow* window;

	fs_thread(wxWindow* window, fs_node* node, vector<fs_thread*>* threads, wxCriticalSection* threads_cs)
		: wxThread(wxTHREAD_DETACHED)
	{
		this->node = node;
		this->threads = threads;
		this->threads_cs = threads_cs;
		this->window = window;
	}

	~fs_thread()
	{
		wxCriticalSectionLocker enter(*threads_cs);
		for (size_t i = 0; i < threads->size(); i++)
		{
			if (threads->at(i) == this)
				threads->at(i) = null;

		}
		//thread = null;
	}

	wxULongLong get_size(const wxString& path)
	{
		Node_Type type = get_node_type(path);
		if (!exists(type))
		{
			//con << "Error getting size for: " << path << endl;
			return wxInvalidSize;
		}

		switch (type)
		{
		case Node_Type::symlink_file:
			return 0;

		case Node_Type::normal_file:
			return wxFileName::GetSize(path);

		case Node_Type::symlink_directory:
			return 0;

		case Node_Type::normal_directory:
		{
			wxULongLong node_size = 0;
			wxDir dir(path);
			if (!dir.IsOpened())
			{
				//cerr << "Failed to open dir" << endl;
				return wxInvalidSize;
			}
			if (TestDestroy()) 
				return wxInvalidSize;
			const wxString basepath = dir.GetNameWithSep();
			wxString filename;
			
			bool has_file = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
			while (has_file)
			{
				if (TestDestroy()) 
					return wxInvalidSize;
				auto sub_size = get_size(basepath + filename);
				if (sub_size == wxInvalidSize)
					return sub_size;
				node_size += sub_size;
				has_file = dir.GetNext(&filename);
			}
			return node_size;
			


			//wxArrayString files;

			//size_t num_files = dir.GetAllFiles(path, &files, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
			//for (size_t i = 0; i < num_files; i++)
			//{
			//	wxString& file = files[i];
			//	Node_Type file_type = get_node_type(file);
			//	if (file_type != Node_Type::normal_file)
			//		continue;
			//	auto sub_size = wxFileName::GetSize(file);
			//	node_size += sub_size;
			//}
			//return node_size;
		}
		}
		//con << "ERROR: This should be unreachable." << endl;
		return wxInvalidSize;
	}

	wxThread::ExitCode Entry() override
	{
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;
		
		wxString node_path = node->get_path();
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;
		
		auto size = get_size(node_path);
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;

		if (size == wxInvalidSize)
		{
			// getting the size failed
			App_Event* event = new App_Event(App_Event_IDs::thread_node_size_calc_failed);
			event->node = node;
			wxQueueEvent(window, event);


			//wxCommandEvent thread_event(APP_EVENT_TYPE, (int)App_Event_IDs::thread_node_size_calc_failed);
			//thread_event.SetClientData(node);
			//wxQueueEvent(window, &thread_event);
			return (wxThread::ExitCode) -1;
		}

		{
			// set the new size
			wxCriticalSectionLocker enter(node->size_cs);
			node->size.val = size;
			node->size.state = node_size_state::processing_complete;
		}
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;


		App_Event* event = new App_Event(App_Event_IDs::thread_node_size_calc_finished);
		event->node = node;
		wxQueueEvent(window, event);

		//wxCommandEvent thread_event(APP_EVENT_TYPE, (int)App_Event_IDs::thread_node_size_calc_finished);
		//thread_event.SetClientData(node);
		//wxQueueEvent(window, &thread_event);
		
		//event_a.SetString("data one"); wxPostEvent(this, event_a);

		//auto thread_event = new wxThreadEvent(get_id(id::thread_node_size_calc_finished));
		//thread_event->SetPayload<fs_node*>(node);
		//if (TestDestroy()) return (wxThread::ExitCode)-1;

		//wxQueueEvent(window, thread_event);

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
	int max_threads;

	wxWindow* window;

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

	int get_num_active_threads()
	{
		wxCriticalSectionLocker enter(threads_cs);
		int active_threads = 0;
		for (auto thread : threads)
		{
			if (thread != null)
				active_threads++;
		}
		return active_threads;
	}

	bool spawn_thread(fs_node* node)
	{
		fs_thread* thread = null;
		{
			wxCriticalSectionLocker enter(threads_cs);
			// get the number of threads
			int active_threads = 0;

			int first_free_thread_index = -1;
			for (size_t i = 0; i < threads.size(); i++)
			{
				if (threads[i] != null)
				{
					active_threads++;
					continue;
				}
				// if we reached this point, the thread slot is null (empty)
				if (first_free_thread_index == -1)
				{
					first_free_thread_index = i;
				}
			}

			// no room for the thread, even if we have a slot for it
			if (active_threads >= max_threads)
				return false;

			thread = new fs_thread(window, node, &threads, &threads_cs);

			if (first_free_thread_index >= 0)
			{
				// thread already has a slot
				threads[first_free_thread_index] = thread;
			}
			else
			{
				// no slot for the new thread; push one back
				threads.push_back(thread);
			}
		}

		// update the node size state here, now that we know that 
		//   we know that we have a thread to process it;
		//   and before we run the thread, so that we won't have to
		//   possibly race with the thread to set the state
		{
			wxCriticalSectionLocker enter(node->size_cs);
			node->size.state = node_size_state::processing;
		}
		ValueChanged(wxDataViewItem(node), 2);

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

			// @todo: should we return false here because adding the thread failed?
			//		  or should we return true so that the caller doesn't keep
			//		  readding the same thread that will fail repeatedly?
			return false;

		}
		return true;
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



	fs_model(wxWindow* window, wxString starting_path)
	{
		this->window = window;
		this->max_threads = wxThread::GetCPUCount();

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
			variant = node_type_name(node->type);
		}
		else if (col == 2)
		{
			//switch (node->type)
			//{

			//case Node_Type::normal_directory:
			{
				wxCriticalSectionLocker enter(node->size_cs);
				switch (node->size.state)
				{
				case node_size_state::processing:
				{
					variant = wxString("Processing.");
				} break;

				case node_size_state::processing_complete:
				{
					if (node->size.val == 0)
						variant = wxString("0 B");
					else
						variant = wxFileName::GetHumanReadableSize(node->size.val);
				} break;

				case node_size_state::unable_to_access_all_files:
				{
					if (node->size.val == 0)
						variant = wxString("0 B*");
					else
						variant = wxFileName::GetHumanReadableSize(node->size.val) + wxString("*");
				} break;

				case node_size_state::waiting_processing:
				{
					variant = wxString("Waiting For Processing.");
				} break;
				}					
			}
			/*break;

			case Node_Type::normal_file:
			{
				variant = wxFileName::GetHumanReadableSize(node->size);
			}
			break;

			case Node_Type::symlink_directory:
			case Node_Type::symlink_file:
			{
				variant = wxString("0 B");
			}
			break;
			}*/
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
			// no, we're not gonna try to parse anything here; sorry
			//node->size = variant.GetULongLong();
			return false;
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