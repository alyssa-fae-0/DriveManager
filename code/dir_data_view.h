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

namespace fs_column
{
	enum {
		name = 0,
		type,
		readable_size,
		raw_size,
		num_columns
	};
}

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


	fs_node(fs_node* parent, const wxString& path) : size(node_size_state::waiting_processing, 0)
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

	void refresh()
	{
		auto path = get_path();
		this->type = get_node_type(path);

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

		if (dir_is_inaccessible(path))
			return; // don't do that...

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

		// process all files at once, to minimize time that we lock the children
		{
			wxCriticalSectionLocker enter(this->children_cs);
			for (auto file : files)
			{
				if (file_is_inaccessible(file))
					continue;
				fs_node* node = new fs_node(this, file);
				//nodes_to_calc_size.push_back(node);
				children.push_back(node);
			}
		}
	}
};

wxString to_string(fs_node* node)
{
	wxString str;
	if (node == null)
	{
		return wxString("NULL Node");
	}
	str << "Path: " << node->get_path();
	str << ",\n  Name: " << node->name;
	str << ",\n  Type: " << to_string(node->type);
	str << ",\n  Size State: ";
	{
		wxCriticalSectionLocker enter(node->size_cs);
		str << size_state_name(node->size.state);
		str << ",\n  Size: " << wxFileName::GetHumanReadableSize(node->size.val);
	}
	str << ",\n  Parent Name: ";
	if (node->parent == null)
		str << "NULL";
	else
		str << node->parent->name;
	str << ",\n  Children names: ";
	{
		wxCriticalSectionLocker enter(node->children_cs);
		if (node->children.size() == 0)
			str << "NULL";
		else
		{
			// actually has children
			for (auto child : node->children)
			{
				str << "\n    " << child->name;
			}
		}
	}
	return str;
}

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


	node_size get_size(const wxString& path)
	{
		Node_Type type = get_node_type(path);
		if (!exists(type))
		{
			//con << "Error getting size for: " << path << endl;
			return node_size(node_size_state::unable_to_access_all_files, 0);
		}

		switch (type)
		{
		case Node_Type::symlink_file:
			return node_size(node_size_state::processing_complete, 0);

		case Node_Type::normal_file:
		{
			auto size = win_get_size(path);
			//auto size = wxFileName::GetSize(path);
			//if (size.state == node_size_state::unable_to_access_all_files)
			//	return size;
			return size;
		}

		case Node_Type::symlink_directory:
			return node_size(node_size_state::processing_complete, 0);

		case Node_Type::normal_directory:
		{
			if (dir_is_inaccessible(path))
				return node_size(node_size_state::unable_to_access_all_files, 0);

			node_size dir_size(node_size_state::processing, 0);
			wxDir dir(path);

			if (!dir.IsOpened())
			{
				//cerr << "Failed to open dir" << endl;
				return node_size(node_size_state::unable_to_access_all_files, 0);
			}

			if (TestDestroy())
				return node_size(node_size_state::unable_to_access_all_files, wxInvalidSize);
			const wxString basepath = dir.GetNameWithSep();

			wxString filename;
			bool has_file = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
			while (has_file)
			{
				if (TestDestroy())
					return node_size(node_size_state::unable_to_access_all_files, wxInvalidSize);
				auto sub_size = get_size(basepath + filename);
				if (sub_size.state == node_size_state::unable_to_access_all_files)
					dir_size.state = node_size_state::unable_to_access_all_files;
				auto subval = sub_size.val;
				if (subval != wxInvalidSize)
					dir_size.val += subval;

				has_file = dir.GetNext(&filename);
			}

			return dir_size;



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
		return node_size(node_size_state::unable_to_access_all_files, 0);
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

		if (size.val == wxInvalidSize)
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
			node->size.val = size.val;
			if (size.state == node_size_state::unable_to_access_all_files)
				node->size.state = size.state;
			else
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

	// @TODO this should probably have a critical section as well
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
		ValueChanged(wxDataViewItem(node), fs_column::raw_size);
		ValueChanged(wxDataViewItem(node), fs_column::readable_size);

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

	fs_node* get_node(const wxString& path)
	{
		bool case_sensitive = false;

		auto path_name = wxFileName(path);
		auto volume = path_name.GetVolume() + ":";
		fs_node* node = null;
		for (fs_node* root : roots)
		{
			if (root->name.IsSameAs(volume, !case_sensitive))
			{
				node = root;
				break;
			}
		}
		if (node == null)
		{
			// can't find the root; create it? return null?
			return null;
		}

		auto dirs = path_name.GetDirs();
		int num_dirs = dirs.GetCount();
		for (int dir_i = 0; dir_i < num_dirs; dir_i++)
		{
			wxCriticalSectionLocker enter(node->children_cs);
			bool child_found = false;
			int num_children = node->children.size();
			for (int child_i = 0; child_i < num_children && !child_found; child_i++)
			{
				auto child = node->children[child_i];
				if (child->name.IsSameAs(dirs[dir_i], !case_sensitive))
				{
					node = child;
					child_found = true;
				}
			}
			if (!child_found)
				return null;
		}

		// all dirs found; check for filename
		if (path_name.HasName())
		{
			auto name = path_name.GetFullName();
			bool found = false;
			wxCriticalSectionLocker enter(node->children_cs);
			int num_children = node->children.size();
			for (int i = 0; i < num_children; i++)
			{
				auto child = node->children[i];
				if (child->name.IsSameAs(name, false))
				{
					found = true;
					node = child;
					break;
				}
			}
			if (!found)
				return null;
		}

		// node should be correct directory or file
		return node;
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
		return fs_column::num_columns;
	}

	// return type as reported by wxVariant
	virtual wxString GetColumnType(unsigned int col) const override
	{
		if (col == fs_column::raw_size)
			return wxT("ulonglong");
		return wxT("string");
	}


	// get value into a wxVariant
	void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override
	{
		assert(item.IsOk());
		fs_node* node = (fs_node*)item.GetID();

		switch (col)
		{
		case fs_column::name:
		{
			variant = get_name(node->name);
		} break;

		case fs_column::type:
		{
			variant = to_string(node->type);
		} break;

		case fs_column::readable_size:
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
		} break;

		case fs_column::raw_size:
		{
			wxULongLong val = 0;
			{
				wxCriticalSectionLocker enter(node->size_cs);
				val = node->size.val;
			}
			if (val == wxInvalidSize)
				variant = (wxULongLong)0;
			else
				variant = val;
		} break;

		default:
		{
			con << "ERROR: model does not have a column: " << col << endl;
			return;
		} break;
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
		//return 0;
		assert(item1.IsOk() && item2.IsOk());
		fs_node* node1 = (fs_node*)item1.GetID();
		fs_node* node2 = (fs_node*)item2.GetID();

		if(column == 0) // name
		{
			wxString name1 = node1->name.Upper();
			wxString name2 = node2->name.Upper();

			if (ascending)
			{
				auto result = name1.compare(name2);
				return result;
			}
			else
			{
				auto result = name2.compare(name1);
				return result;
			}
		}
		/*else if (column == fs_column::raw_size) // size
		{
			//item1.
			wxULongLong size1 = 0;
			wxULongLong size2 = 0;
			{
				wxCriticalSectionLocker node1_cs(node1->size_cs);
				size1 = node1->size.val;
			}

			{
				wxCriticalSectionLocker node2_cs(node2->size_cs);
				size1 = node1->size.val;
			}

			int size1_greater = -1;
			int size2_greater = 1;
			int sizes_equal = 0;

			if (size1 == wxInvalidSize && size2 != wxInvalidSize)
				return size2_greater;

			if (size1 != wxInvalidSize && size2 == wxInvalidSize)
				return size1_greater;

			if (size1 == wxInvalidSize && size2 == wxInvalidSize)
				return sizes_equal;

			if (size1 > size2)
				return size1_greater;

			if (size2 > size1)
				return size2_greater;

			return sizes_equal;
		}*/
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