#pragma once
#include "fae_lib.h"
#include "new_filesystem.h"
#include "app_events.h"

struct Fs_node;
extern deque<Fs_node*> nodes_to_calc_size;
extern wxCriticalSection nodes_to_calc_size_cs;

extern vector<Fs_node*> recycled_nodes;
extern wxCriticalSection recycled_nodes_cs;

#define locker wxCriticalSectionLocker


void recycle(Fs_node* node)
{
	locker enter(recycled_nodes_cs);
	recycled_nodes.push_back(node);
}

Fs_node* get_recycled_node()
{
	Fs_node* node = null;
	locker enter(recycled_nodes_cs);
	if (recycled_nodes.size() > 0)
	{
		node = recycled_nodes.back();	
		recycled_nodes.pop_back();
	}
	return node;
}

namespace Fs_column
{
	enum {
		name = 0,
		type,
		readable_size,
		raw_size,
		num_columns
	};
}


struct Fs_node
{

	//u32 uid;
	wxString name;
	Node_type type;

	Node_size size;

	Fs_node* parent;
	vector<Fs_node*> children;
	wxCriticalSection lock;

	Fs_node()
	{

	}

	Fs_node(Fs_node* parent, const wxString& path)
	{
		init(parent, path);
	}

	void init(Fs_node* parent, const wxString& path)
	{
		//con << "Creating node: " << path << endl;
		//this->uid = uid;
		this->parent = parent;
		this->name = get_name(path);

		this->type = get_node_type(path);
		// if (type == Node_Type::nt_normal_directory) con << "calculating size for " << path << endl;

		this->size = Node_size(node_size_state::waiting_processing, 0);

		if (this->type != Node_type::normal_directory)
		{
			auto val = get_size(path);
			this->size.val = val;
			this->size.state = node_size_state::processing_complete;
		}
		else
		{
			{
				auto val = wxInvalidSize;
				size.val = val;
				size.state = node_size_state::waiting_processing;
			}
			{
				locker enter(nodes_to_calc_size_cs);
				nodes_to_calc_size.push_back(this);
			}
		}

		//if (type == Node_Type::nt_normal_directory) con << "size calculated" << endl;
	}

	void remove(wxDataViewItemArray& deleted_items)
	{
		// remove any children
		for (int i = 0, num_children = children.size(); i < num_children; i++)
		{
			Fs_node* child = children[i];
			if (child != null)
			{
				locker enter(child->lock);
				child->remove(deleted_items);
				children[i] = null;
				recycle(child);
			}
		}

		// add this node to the list after its children (if applicable)
		deleted_items.push_back((wxDataViewItem)this);
	}

	void refresh(wxDataViewItemArray& changed_items, wxDataViewItemArray& deleted_items)
	{
		bool type_changed = false;
		bool size_changed = false;

		auto path = get_path();
		auto new_type = get_node_type(path);

		if (new_type != this->type)
		{
			type_changed = true;
			this->type = new_type;
		}

		if (this->type != Node_type::normal_directory)
		{
			auto val = get_size(path);
			if (this->size.val != val)
			{
				size_changed = true;
			}
			this->size.val = val;
			this->size.state = node_size_state::processing_complete;
		}
		else
		{
			{
				auto val = wxInvalidSize;
				size.val = val;
				size.state = node_size_state::waiting_processing;
			}
			{
				locker enter(nodes_to_calc_size_cs);
				nodes_to_calc_size.push_back(this);
			}
		}

		if (size_changed || type_changed)
		{
			changed_items.push_back((wxDataViewItem)this);
			if (this->type != Node_type::normal_directory)
			{
				// remove any children
				for(int i = 0, num_children = children.size(); i < num_children; i++)
				{
					Fs_node* child = children[i];
					if (child != null)
					{
						child->remove(deleted_items);
						locker enter(child->lock);
						children[i] = null;
						recycle(child);
					}
				}
				children.clear();
			}
		}
	}

	~Fs_node()
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
			locker enter(parent->lock);
			parent->build_path(path);
		}
		path.append(name);
		path.append(wxFileName::GetPathSeparator());
	}

	wxString get_path()
	{
		wxString path;
		build_path(path);
		//con << "Built path: " << path << endl;
		return path;
	}

	// @todo: should this be folded into the thread traversing?
	// @todo @robustness: add better checks to only update children when we actually need to
	// @hack
	void update_children()
	{
		if (children.size() != 0)
		{
			return;
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
		
		for (auto file : files)
		{
			if (file_is_inaccessible(file))
				continue;
			Fs_node* node = get_recycled_node();
			if (node != null)
			{
				locker enter(node->lock);
				node->init(this, file);
			}
			else
			{
				node = new Fs_node(this, file);
			}
			children.push_back(node);
		}
	}
};

Fs_node* new_node()
{
	Fs_node* node = get_recycled_node();
	if (node == null)
		node = new Fs_node();
	return node;
}


wxString to_string(Fs_node* node)
{	
	if (node == null)
	{
		return wxString("NULL Node");
	}

	locker enter(node->lock);
	wxString str;
	str << "Path: " << node->get_path();
	str << ",\n  Name: " << node->name;
	str << ",\n  Type: " << to_string(node->type);
	str << ",\n  Size State: " << to_string(node->size.state);
	str << ",\n  Size: " << wxFileName::GetHumanReadableSize(node->size.val);
	str << ",\n  Parent Name: ";
	if (node->parent == null)
		str << "NULL";
	else
		str << node->parent->name;

	str << ",\n  Children names: ";
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
	
	return str;
}

struct Fs_thread : public wxThread
{

	Fs_node* node;
	vector<Fs_thread*>* threads;
	wxCriticalSection* threads_cs;
	wxWindow* window;

	Fs_thread(wxWindow* window, Fs_node* node, vector<Fs_thread*>* threads, wxCriticalSection* threads_cs)
		: wxThread(wxTHREAD_DETACHED)
	{
		this->node = node;
		this->threads = threads;
		this->threads_cs = threads_cs;
		this->window = window;
	}

	~Fs_thread()
	{
		locker enter(*threads_cs);
		for (size_t i = 0; i < threads->size(); i++)
		{
			if (threads->at(i) == this)
				threads->at(i) = null;

		}
		//thread = null;
	}


	Node_size get_size(const wxString& path)
	{
		Node_type type = get_node_type(path);
		if (!exists(type))
		{
			//con << "Error getting size for: " << path << endl;
			return Node_size(node_size_state::unable_to_access_all_files, 0);
		}

		switch (type)
		{
		case Node_type::symlink_file:
			return Node_size(node_size_state::processing_complete, 0);

		case Node_type::normal_file:
		{
			auto size = win_get_size(path);
			//auto size = wxFileName::GetSize(path);
			//if (size.state == node_size_state::unable_to_access_all_files)
			//	return size;
			return size;
		}

		case Node_type::symlink_directory:
			return Node_size(node_size_state::processing_complete, 0);

		case Node_type::normal_directory:
		{
			if (dir_is_inaccessible(path))
				return Node_size(node_size_state::unable_to_access_all_files, 0);

			Node_size dir_size(node_size_state::processing, 0);
			wxDir dir(path);

			if (!dir.IsOpened())
			{
				//cerr << "Failed to open dir" << endl;
				return Node_size(node_size_state::unable_to_access_all_files, 0);
			}

			if (TestDestroy())
				return Node_size(node_size_state::unable_to_access_all_files, wxInvalidSize);
			const wxString basepath = dir.GetNameWithSep();

			wxString filename;
			bool has_file = dir.GetFirst(&filename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN | wxDIR_NO_FOLLOW);
			while (has_file)
			{
				if (TestDestroy())
					return Node_size(node_size_state::unable_to_access_all_files, wxInvalidSize);
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
		return Node_size(node_size_state::unable_to_access_all_files, 0);
	}

	wxThread::ExitCode Entry() override
	{
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;
		
		wxString node_path;
		{
			locker enter(node->lock);
			 node_path = node->get_path();
		}
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;
		
		auto size = get_size(node_path);
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;

		if (size.val == wxInvalidSize)
		{
			// getting the size failed
			App_event* event = new App_event(App_event_id::thread_node_size_calc_failed);
			event->node = node;
			wxQueueEvent(window, event);


			//wxCommandEvent thread_event(APP_EVENT_TYPE, (int)App_Event_IDs::thread_node_size_calc_failed);
			//thread_event.SetClientData(node);
			//wxQueueEvent(window, &thread_event);
			return (wxThread::ExitCode) -1;
		}

		{
			// set the new size
			locker enter(node->lock);
			node->size.val = size.val;
			if (size.state == node_size_state::unable_to_access_all_files)
				node->size.state = size.state;
			else
				node->size.state = node_size_state::processing_complete;
		}
		if (TestDestroy()) 
			return (wxThread::ExitCode)-1;


		App_event* event = new App_event(App_event_id::thread_node_size_calc_finished);
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

enum struct Node_operation_type : int
{
	restore,
	relocate
};

struct Node_operation
{
	Fs_node* node;
	Node_operation_type type;
};

struct Fs_model : public wxDataViewModel
{

	// @TODO this should probably have a critical section as well
	vector<Fs_node*> roots;

	// @note:	this is the centralized uid source; all uids for this model come from here;
	//			uids can't be returned, so if we end up implementing a system for getting rid
	//			of nodes, we shouldnt delete the nodes (or we'll risk running out of uids);
	//			maybe we should use a "freed nodes" list and have freed nodes added to that
	u32 cur_uid;
	wxCriticalSection cur_uid_cs;

	// this is the source of all of the threads in the app; it uses a critical section to synchronize
	// additions and removals of threads (I think that's needed so that we don't run into a situation
	// where we have e.g. two threads trying to reclaim an empty slot at the same time?)
	vector<Fs_thread*> threads;
	wxCriticalSection threads_cs;
	int max_threads;

	queue<Node_operation> queued_ops;


	wxWindow* window;

	void queue_node(Fs_node* node)
	{
		assert(node != null);
		Node_operation op;
		op.node = node;

		{
			locker enter(node->lock);
			if (is_symlink(node->type))
				op.type = Node_operation_type::restore;
			else if (is_normal(node->type))
				op.type = Node_operation_type::relocate;
			else
				assert(false);
		}
		queued_ops.push(op);
	}

	void refresh(Fs_node* node)
	{
		wxDataViewItemArray updated_items;
		wxDataViewItemArray deleted_items;
		{
			locker enter(node->lock);
			node->refresh(updated_items, deleted_items);
		}
		if(deleted_items.Count() != 0)
			this->ItemsDeleted(wxDataViewItem(node), deleted_items);
		//if(updated_items.Count() != 0)
		//	this->ItemsChanged(updated_items);

		// @hack this ugly hack is to fix the issue with updated items not getting their
		//	container status updated
		this->ItemDeleted((wxDataViewItem)node->parent, (wxDataViewItem)node);
		this->ItemAdded((wxDataViewItem)node->parent, (wxDataViewItem)node);
	}

	u32 get_new_uid() 
	{
		u32 get_id = 0;
		// make the time that we hold on to the cur_uid as short as possible
		{
			locker enter(cur_uid_cs);
			get_id = cur_uid++;
		}
		return get_id;
	}

	int get_num_active_threads()
	{
		locker enter(threads_cs);
		int active_threads = 0;
		for (auto thread : threads)
		{
			if (thread != null)
				active_threads++;
		}
		return active_threads;
	}

	bool spawn_thread(Fs_node* node)
	{
		Fs_thread* thread = null;
		{
			locker enter(threads_cs);
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

			thread = new Fs_thread(window, node, &threads, &threads_cs);

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
			locker enter(node->lock);
			node->size.state = node_size_state::processing;
		}
		ItemChanged(wxDataViewItem(node));

		if (thread->Run() != wxTHREAD_NO_ERROR)
		{
			con << "ERROR: Failed to create thread" << endl;
			delete thread;

			locker enter(threads_cs);
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
			locker enter(threads_cs);
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
				locker enter(threads_cs);
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

	bool is_size_valid(Node_size& size)
	{
		if (size.state != node_size_state::processing_complete)
			return false;
		if (size.val == wxInvalidSize)
			return false;
		return true;
	}

	void jump_node_to_front_of_update_queue(Fs_node* node)
	{
		locker enter(nodes_to_calc_size_cs);
		// @TODO: this is an ugly mess. Fix this
		for (auto iter = nodes_to_calc_size.begin(); iter < nodes_to_calc_size.end(); ++iter)
		{
			if (*iter == node)
			{
				nodes_to_calc_size.erase(iter);

				// reset the iterator so we can remove any duplicate entries
				iter = nodes_to_calc_size.begin(); 
			}
		}
		nodes_to_calc_size.push_front(node);
	}


	Result relocate_node(Fs_node* node)
	{
		assert(node != null);
		Node_type node_type = Node_type::not_exist;
		Node_size node_size;
		wxString node_path;
		{
			locker enter(node->lock);
			node_type = node->type;
			node_size = node->size;
			node_path = node->get_path();
		}

		if (node_type != Node_type::normal_directory &&
			node_type != Node_type::normal_file)
			return {false, wxString("Node is ") << to_string(node_type) << " which cannot be relocated."};
		if (!is_size_valid(node_size))
		{
			jump_node_to_front_of_update_queue(node);
			return { false, wxString("Node's size couldn't be calculated. Size_State: ") << to_string(node_size.state) };
		}

		auto bak_dir_path = Settings.backup_dir.GetFullPath();
		if (!wxFileName::DirExists(bak_dir_path))
			if (!create_dir_recursively(bak_dir_path))
				return {false, wxString("No backup directory found, and couldn't be created. Backup dir: ") << bak_dir_path};

		auto bak_drive_space = get_drive_space(bak_dir_path);

		if (bak_drive_space <= node_size.val)
			return { false, wxString("Not enough space. Bakup Drive Free Space: ")
			<< wxFileName::GetHumanReadableSize(bak_drive_space) << ", Node Size: "
			<< wxFileName::GetHumanReadableSize(node_size.val) };

		// create the full target name
		wxString dest = bak_dir_path;
		wxString node_name = get_name(node_path);

		
		if (!wxFileName::IsPathSeparator(dest.Last()))
			dest.append(wxFileName::GetPathSeparator());
		dest.append(node_name);
		remove_trailing_slash(dest);

		wxString src = node_path;
		remove_trailing_slash(src);

		con << "Source: " << node_path << endl;
		con << "Target: " << dest << endl;

		// make sure that we're not trying to backup anything up into the backup dir directly
		// we don't want to delete the backup_dir!!!
		if (Settings.backup_dir.SameAs(wxFileName(dest)))
			return { false, wxString() << "Error: dest: " << dest << " is same location as backup directory" << Settings.backup_dir.GetFullPath() };

		// there shouldn't be anything in the destination
		if(get_node_type(dest) != Node_type::not_exist)
			return { false, wxString() << "Error: dest: " << dest << " is already occupied." };
		
		// src (c:\dev\test_data) has the data, and dst (d:\bak\test_data) is empty
		// copy src to dst
		if (!copy_recursive(src, dest))
			return { false, wxString() << "Error: failed to copy " << src << " to " << dest };

		// src (c:\dev\test_data) and dst (d:\bak\test_data) now have full copies of the data
		// delete src's copy
		if (!delete_target(src))
			return { false, wxString() << "Error: failed to delete " << src };
		

		// src (c:\dev\test_data) is empty and dst (d:\bak\test_data) now has the data
		// create a symlink in src that points to dst
		if (!create_symlink(src, dest))
			return { false, wxString() << "Error: failed to create symlink from " << src << " to " << dest };
		

		// src (c:\dev\test_data) contains a symlink to dst (d:\bak\test_data) which has the data
		// and we're done!

		refresh(node);
		ItemChanged((wxDataViewItem)node);
		return { true, "" };
	}

	Result restore_node(Fs_node* node)
	{
		assert(node != null);
		Node_type node_type = Node_type::not_exist;
		wxString node_path;
		{
			locker enter(node->lock);
			node_type = node->type;
			node_path = node->get_path();
		}

		if (node_type != Node_type::symlink_directory &&
			node_type != Node_type::symlink_file)
			return { false, wxString("Node is ") << to_string(node_type) << " which cannot be restored." };

		//if (!is_size_valid(node_size))
		//	return { false, wxString("Node's size couldn't be calculated. Size_State: ") << to_string(node_size.state) };

		wxString target_path;
		if(!get_target_of_symlink(node_path, target_path))
			return { false, wxString("Unable to get target of node: ") << node_path };

		auto node_drive_space = get_drive_space(node_path);
		if (node_drive_space == wxInvalidSize)
			return { false, wxString("Unable to get drive space for: ") << node_path };

		auto target_size = get_size(target_path);
		if (target_size == wxInvalidSize)
			return { false, wxString("Unable to calculate size of: ") << target_path };

		if (node_drive_space <= target_size)
			return { false, wxString("Not enough space. Node Drive Free Space: ")
			<< wxFileName::GetHumanReadableSize(node_drive_space) << ", Target Size: "
			<< wxFileName::GetHumanReadableSize(target_size) };

		wxString src = node_path;
		remove_trailing_slash(src);

		wxString target = target_path;
		remove_trailing_slash(target);

		// source (c:\dev\test_data -> d:\bak\test_data)
		// delete symlink
		if (!delete_target(src))
			return { false, wxString() << "Error: Failed to delete node: " << src };

		// copy data from target back to source
		if (!copy_recursive(target, src))
			return { false, wxString() << "Failed to copy: " << target << " to " << src };
		
		// delete target's copy
		if (!delete_target(target))
			return { false, wxString() << "Failed to delete: " << target };

		// and we're done

		refresh(node);
		ItemChanged((wxDataViewItem)node);
		return { true, "" };
	}

	Fs_node* get_node(const wxString& path)
	{
		bool case_sensitive = false;

		auto path_name = wxFileName(path);
		auto volume = path_name.GetVolume() + ":";
		Fs_node* node = null;
		for (Fs_node* root : roots)
		{
			locker enter(root->lock);
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

		locker enter(node->lock);
		auto dirs = path_name.GetDirs();
		int num_dirs = dirs.GetCount();

		// @robustness: this may be vulnerable to node deletion
		for (int dir_i = 0; dir_i < num_dirs; dir_i++)
		{
			bool child_found = false;
			int num_children = node->children.size();
			for (int child_i = 0; child_i < num_children && !child_found; child_i++)
			{
				auto child = node->children[child_i];
				locker child_enter(child->lock);
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
			int num_children = node->children.size();
			for (int i = 0; i < num_children; i++)
			{
				auto child = node->children[i];
				locker child_enter(child->lock);
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

	Fs_model(wxWindow* window, wxString starting_path)
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
			roots.push_back(new Fs_node(null, drive));
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

	~Fs_model()
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
		return Fs_column::num_columns;
	}

	// return type as reported by wxVariant
	virtual wxString GetColumnType(unsigned int col) const override
	{
		if (col == Fs_column::raw_size)
			return wxT("ulonglong");
		return wxT("string");
	}


	// get value into a wxVariant
	void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override
	{
		assert(item.IsOk());
		Fs_node* node = (Fs_node*)item.GetID();
		if (node == null)
			return;
		locker enter(node->lock);

		switch (col)
		{
		case Fs_column::name:
		{
			variant = get_name(node->name);
		} break;

		case Fs_column::type:
		{
			variant = to_string(node->type);
		} break;

		case Fs_column::readable_size:
		{
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

		case Fs_column::raw_size:
		{
			wxULongLong val = 0;
			{
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
		Fs_node* node = (Fs_node*)item.GetID();
		if (node == null)
			return false;
		locker enter(node->lock);
		if (col == 0)
		{
			node->name = variant.GetString();
			return true;
		}
		else if (col == 1)
		{
			node->type = (Node_type)variant.GetLong();
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

		Fs_node* node = (Fs_node*)item.GetID();
		if (node == null)
			return wxDataViewItem(null);
		locker enter(node->lock);
		return wxDataViewItem((void*)node->parent);
	}

	bool IsContainer(const wxDataViewItem &item) const override
	{
		Fs_node* node = (Fs_node*)item.GetID();
		if (!node)
			return true;

		locker enter(node->lock);
		return node->type == Node_type::normal_directory;
	}

	unsigned int GetChildren(const wxDataViewItem &item, wxDataViewItemArray &children) const override
	{
		Fs_node* node = (Fs_node*)item.GetID();
		if (!node)
		{
			for(auto root : roots)
				children.Add(wxDataViewItem((void*)root));
			return roots.size();
		}
		locker enter(node->lock);
		size_t num_children = node->children.size();
		for (size_t i = 0; i < num_children; i++)
		{
			Fs_node* child = node->children.at(i);
			children.Add(wxDataViewItem((void*)child));
		}
		return num_children;
	}


	virtual int Compare(const wxDataViewItem &item1, const wxDataViewItem &item2, unsigned int column, bool ascending) const override
	{
		//return 0;
		assert(item1.IsOk() && item2.IsOk());
		Fs_node* node1 = (Fs_node*)item1.GetID();
		Fs_node* node2 = (Fs_node*)item2.GetID();

		if (node1 == null || node2 == null)
			return 0;

		locker enter1(node1->lock);
		locker enter2(node2->lock);

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
				locker node1_cs(node1->size_cs);
				size1 = node1->size.val;
			}

			{
				locker node2_cs(node2->size_cs);
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