#include "stdafx.h"
#include "fae_lib.h"
#include "fae_string.h"
#include "console.h"
#include "new_filesystem.h"
#include "dir_data_view.h"
#include "app_events.h"

/*

@Assumptions:
	A backup directory shouldn't already exist when a file is relocated
	Example: for C:\dev\test_data, c:\dev\bak\test_data MUST NOT EXIST (currently)
		Maybe we should check if the directory is empty and allow the move if the directory is empty
			(Notify the user if we do)

	When relocating files/folders and we run into symlinked files/folders, we have some options:
		1. Copy the files and folders as regular files and folders
			This will duplicate data and break anything that requires that the files/folders are symlinked
				So we should probably NOT do this
		2. Copy the files and folders as symlinks, retaining their targets
			This could be a problem if end up copying the targets, as the symlinks wouldn't point at the correct location anymore
				(I think. I need to look deeper into how symlinks work on windows)
		3. Copy the targets to the backup location first, and then copy the files and folders as symlinks pointing to the new location
			This would be the safest option, I think.
		Gonna attempt option 2.



@TODO list:
	@Bug: Debugging access violation exception thrown when a directory is expanded
		It only happens for directories that are partially access-denied (their size are marked with an
		*) An example: C:\Program Data\Microsoft\Diagnostics (Diagnosis?)
	@Bug: I can't figure out how to sort the display items by size (because their sizes are constantly changing)
		I could theoretically add a sizes_frozen variable to the model and only sort when all sizes are frozen
		Or I could adjust the readable_size compare to parse the readable size string into a number for the comparison
	@Feature: Implement the display with Sashes, so users can adjust element sizes
	@Feature @Bug @Expand: the fs_threads currently only calculate size, rather than creating nodes
		for all of the files/directories they visit. Not sure if this will end up being better or worse than
		just calculating sizes as directories are opened


	@Robustness? Add function for checking if files are identical, once I add file processing
	@Bug Figure out why I can't "cd c:\" but I can "cd c:\dev"

@NEXT:
	Add existing functionality to the GUI
		Add support to Drag-and-drop folders onto the window to relocate/restore them

*/

//wxDEFINE_EVENT(APP_EVENT_TYPE, wxCommandEvent);

App_Settings Settings;

//struct Test_Result
//{
//	const char* expression;
//	bool success;
//};
//
//vector<Test_Result> tests;
//
//#define Test(name) void name() 
//#define Require(expression) tests.push_back({#expression, expression});
//#define test_are_equal(a,b) 

// As before, define a new wxEventType
wxDEFINE_EVENT(App_Event_Type, App_Event);


void run_tests()
{
	// node_type tests
	{
		assert(get_node_type("C:\\") == Node_Type::normal_directory);
		assert(get_node_type("C:") == Node_Type::normal_directory);
		assert(get_node_type("C") == Node_Type::not_exist);

		// @BUG: these should be correct... @TODO: look into why I can call function
		//		 on "C:\\" but not "D:\\"
		//assert(get_node_type("D:\\") == Node_Type::nt_normal_directory);
		//assert(get_node_type("D:") == Node_Type::nt_normal_directory);
		//assert(get_node_type("D") == Node_Type::nt_not_exist);
	}

	{
		bool accessible = dir_is_inaccessible("C:\\dev\\");
	}




	// get_name test
	{
		wxString result = "test_data";
		assert(get_name("C:\\dev\\test_data") == result);
		assert(get_name("C:\\dev\\test_data\\") == result);
	}

	// get_size test
	{
		assert(get_size(Settings.test_data_source.GetFullPath()) == 759949);
	}
}

enum struct id : int
{
	hello = 0,
	open_dir_picker,
	relocate_dir_picker,
	test_button,
	console_id,
	scroll_button,
	thread_count_timer_id,
	num_ids
};

int IDs[(int)id::num_ids];

int get_id(id num)
{
	assert((int)num >= 0 && num < id::num_ids);
	return IDs[(int)num];
}

void init_IDs()
{
	for (int i = 0; i < (int)id::num_ids; i++)
	{
		IDs[i] = wxNewId();
	}
}

wxTextCtrl* text_console = null;
ostream* Console = null;
wxCriticalSection con_cs;
wxObjectDataPtr<fs_model> Model;
vector<fs_node*> nodes_to_calc_size;
wxCriticalSection nodes_to_calc_size_cs;


class MyFrame : public wxFrame
{

public:

	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
		: wxFrame(NULL, wxID_ANY, title, pos, size)
	{
		data_view = null;
		model = null;

		init_settings();
		init_IDs();
		//set_IDs();
		text_console = new wxTextCtrl(this, get_id(id::console_id), "Console Log:\n", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP);
		text_console->SetMaxSize(wxSize(-1, 150));
		Console = new std::ostream(text_console);

		run_tests();


		Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnClose, this);
		Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
		Bind(wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT);
		Bind(wxEVT_MENU, &MyFrame::OnHello, this, get_id(id::hello));
		Bind(wxEVT_MENU, &MyFrame::OnDirPicker, this, get_id(id::open_dir_picker));
		Bind(wxEVT_DIRPICKER_CHANGED, &MyFrame::OnDirPickerChanged, this, get_id(id::relocate_dir_picker));
		Bind(wxEVT_BUTTON, &MyFrame::on_test_button, this, get_id(id::test_button));
		//Bind(wxEVT_BUTTON, &MyFrame::on_scroll_button, this, ID.scroll_button);


		wxMenu *menuFile = new wxMenu;
		menuFile->Append(get_id(id::hello), "&hello...\tCtrl-H", "Help string shown in status bar for this menu item");
		menuFile->AppendSeparator();
		menuFile->Append(wxID_EXIT);

		wxMenu *menuHelp = new wxMenu;
		menuHelp->Append(wxID_ABOUT);

		//wxMenu *menuDir = new wxMenu;
		//menuDir->Append(ID.open_dir_picker, "Show Dir Picker", "Show Dir Picker");

		wxMenuBar *menuBar = new wxMenuBar;
		menuBar->Append(menuFile, "&File");
		menuBar->Append(menuHelp, "&Help");
		//menuBar->Append(menuDir,  "&Dir");
		SetMenuBar(menuBar);
		CreateStatusBar();
		SetStatusText("Welcome to wxWidgets!");


		wxBoxSizer* window_sizer = new wxBoxSizer(wxVERTICAL);

		// Settings
		wxBoxSizer* settings_sizer = new wxBoxSizer(wxHORIZONTAL);
		settings_sizer->Add(
			new wxStaticText(this, wxID_ANY, "Backup Directory:", wxDefaultPosition, wxDefaultSize),
			0, 0, 0);

		settings_sizer->AddSpacer(10);

		settings_sizer->Add(
			new wxDirPickerCtrl(this, get_id(id::relocate_dir_picker), Settings.backup_dir.GetFullPath(), wxDirSelectorPromptStr, wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL),
			1, wxEXPAND, 0);

		//settings_sizer->SetSizeHints(this);
		window_sizer->Add(settings_sizer, 0, wxEXPAND, 0);

		// data view control dataviewctrl
		{
			auto panel = new wxPanel(this, wxID_ANY);

			// Navigation View
			data_view = new wxDataViewCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES);
			model = new fs_model(this, "C:\\dev");
			//model = new fs_model(Settings.test_data_source.GetFullPath());
			Model = model;
			data_view->AssociateModel(model.get());

			auto text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col0 = new wxDataViewColumn("Path", text_renderer, fs_column::name, 350,
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			data_view->AppendColumn(col0);

			col0->SetSortOrder(true);

			text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col1 = new wxDataViewColumn("Node Type", text_renderer, fs_column::type, 170,
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			data_view->AppendColumn(col1);

			text_renderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
			wxDataViewColumn* col2 = new wxDataViewColumn("Size", text_renderer, fs_column::readable_size, 120,
				wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			data_view->AppendColumn(col2);

			//text_renderer = new wxDataViewTextRenderer("ulonglong", wxDATAVIEW_CELL_INERT);
			//wxDataViewColumn* col3 = new wxDataViewColumn("Raw Size", text_renderer, fs_column::raw_size, 80,
			//	wxALIGN_LEFT, wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
			//data_view->AppendColumn(col3);

			wxSizer *panel_sizer = new wxBoxSizer(wxVERTICAL);
			data_view->SetMinSize(wxSize(640, 280));
			panel_sizer->Add(data_view, 1, wxGROW | wxALL, 5);

			panel->SetSizerAndFit(panel_sizer);
			window_sizer->Add(panel, 1, wxEXPAND, 0);

			data_view->SetIndent(10);
		}

		Bind(wxEVT_DATAVIEW_ITEM_EXPANDING, &MyFrame::on_data_view_item_expanding, this);
		Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MyFrame::on_model_item_double_clicked, this);
		//Bind(wxEVT_DATAVIEW_COLUMN_SORTED, &MyFrame::on_model_resort, this);

		Bind(App_Event_Type, &MyFrame::on_nodes_added_to_model, this, (int)App_Event_IDs::nodes_added);
		Bind(App_Event_Type, &MyFrame::on_node_size_calc_failed, this, (int)App_Event_IDs::thread_node_size_calc_failed);
		Bind(App_Event_Type, &MyFrame::on_node_size_calc_finished, this, (int)App_Event_IDs::thread_node_size_calc_finished);

		timer = new wxTimer(this, get_id(id::thread_count_timer_id));
		Bind(wxEVT_TIMER, &MyFrame::on_thread_count_timer, this);

		//Bind(wxEVT_MENU, &MyFrame::OnDirPicker, this, ID.open_dir_picker);
		//Bind(wxEVT_DIRPICKER_CHANGED, &MyFrame::OnDirPickerChanged, this, ID.relocate_dir_picker);
		//Bind(wxEVT_BUTTON, &MyFrame::on_test_button, this, ID.test_button);

		if (false) // directory browser -- buggy with symlinks
		{
			auto dir = new wxGenericDirCtrl(this, -1, wxDirDialogDefaultFolderStr, wxDefaultPosition, wxDefaultSize, wxDIRCTRL_MULTIPLE, "*.*");
			dir->ShowHidden(true);
			dir->SetDefaultPath("c:\\api\\wx\\lib\\vc141_dll\\");
			dir->ExpandPath(dir->GetDefaultPath());
			window_sizer->Add(dir, 1, wxEXPAND, 0);
		}

		// Misc utilities
		window_sizer->Add(new wxButton(this, get_id(id::test_button), "Run Test"), 0, wxEXPAND, 0);
		window_sizer->Add(text_console, 1, wxEXPAND, 0);
		con << endl;

		SetSizerAndFit(window_sizer);
		timer->Start(time_between_thread_counts); // milliseconds
		
		//Bind(wxEVT_SIZE, &MyFrame::on_window_resized, this);
	}

	wxTimer* timer;
	wxDataViewCtrl* data_view;
	wxObjectDataPtr<fs_model> model;
	const int time_between_thread_counts = 500; // miliseconds

	void sort_ctrl(wxDataViewColumn* column, bool ascending)
	{
		auto children = data_view->GetChildren();
	}

	void on_model_resort(wxDataViewEvent& event)
	{
		event.Skip();
		auto column = data_view->GetSortingColumn();
		if (column != null)
		{
			bool ascending = column->IsSortOrderAscending();
			sort_ctrl(column, ascending);
		}
		else
		{
			con << "column was null (sorting)" << endl;
		}
		con << "Model Resorted" << endl;
		event.Veto();
	}

	void on_window_resized(wxSizeEvent& event)
	{
		auto size = event.GetSize();
		con << "Resize: {" << size.GetWidth() << ", " << size.GetHeight() << "}" << endl;
		event.Skip();
	}

	void on_thread_count_timer(wxTimerEvent& event)
	{
		this->SetStatusText(wxString("Active Threads: ") << model->get_num_active_threads());
	}

	void on_node_size_calc_finished(App_Event& event)
	{
		assert(event.GetId() == (int)App_Event_IDs::thread_node_size_calc_finished);
		fs_node* node = event.node;
		//con << "Recalculated size for: " << node->get_path() << endl;
		//{
			//wxCriticalSectionLocker enter(node->size_cs);
			//con << "  New size: " << wxFileName::GetHumanReadableSize(node->size.val) << endl;
		//}
		model->ValueChanged(wxDataViewItem(node), 2);
		wxQueueEvent(this, new App_Event(App_Event_IDs::nodes_added));
	}

	void on_node_size_calc_failed(App_Event& event)
	{
		assert(event.GetId() == (int)App_Event_IDs::thread_node_size_calc_failed);
		fs_node* node = event.node;
		con << "Failed to calculate size for: " << node->get_path() << endl;
		wxQueueEvent(this, new App_Event(App_Event_IDs::nodes_added));
	}

	void on_nodes_added_to_model(App_Event& event)
	{
		assert(event.GetId() == (int)App_Event_IDs::nodes_added);

		{
			wxCriticalSectionLocker enter(nodes_to_calc_size_cs);
			for(int i = nodes_to_calc_size.size() - 1; i >= 0; i--)
			{
				fs_node* node = nodes_to_calc_size.at(i);
				if (model->spawn_thread(node))
					nodes_to_calc_size.pop_back();
				else
					break;
			}
		}
	}

	void on_model_item_double_clicked(wxDataViewEvent& event)
	{
		auto item = event.GetItem();
		assert(item.IsOk());
		fs_node* node = (fs_node*)item.GetID();
		con << to_string(node) << endl;
	}

	void on_test_button(wxCommandEvent& event)
	{
		wxString dir_path = "C:\\dev\\test_data\\";
		fs_node* dir_node = model->get_node(dir_path);
		con << dir_path << ": " << to_string(dir_node) << endl << endl;

		wxString file_path = "C:\\dev\\test_data\\test_a.txt";
		fs_node* file_node = model->get_node(file_path);
		con << file_path << ": " << to_string(file_node) << endl << endl;

		if (dir_node != null)
		{
			if (dir_node->type == Node_Type::normal_directory)
			{
				if (relocate(dir_path))
				{
					con << "Successfully relocated " << dir_path << endl;
					dir_node->refresh();
					model->ItemChanged((wxDataViewItem)dir_node);
					con << dir_path << ": " << to_string(dir_node) << endl;
				}
				else
					con << "Failed to relocate: " << dir_path << endl;
					
			}
			else if (dir_node->type == Node_Type::symlink_directory)
			{
				if (restore(dir_path))
				{
					con << "Successfully restored " << dir_path << endl;
					dir_node->refresh();
					model->ItemChanged((wxDataViewItem)dir_node);
					con << dir_path << ": " << to_string(dir_node) << endl;
				}
				else
					con << "Failed to relocate: " << dir_path << endl;
			}
			else
				con << "ERROR: DIR NODE IS OF TYPE: " << to_string(dir_node->type);
		}

		//con << "Starting tests." << endl;
		//test_list_items_in_dir();
		//test_copy_one_dir_to_another();
		//test_relocate_dir();
		//test_restore_dir();
		//con << "Tests finished successfully." << endl;

		//con << node1->size.val << ", " << node2->size.val << wxDataViewModel::Compare(item1, item2, column, ascending);
	}

	void on_data_view_item_expanding(wxDataViewEvent& event)
	{
		auto item = event.GetItem();
		assert(item.IsOk());
		auto node = (fs_node*)event.GetItem().GetID();
		if (node->type != Node_Type::normal_directory || dir_is_inaccessible(node->get_path()))
		{
			event.Veto();
			return;
		}

		wxString path = node->get_path();
		//con << "on_data_view_item_expanding() path: " << path << endl;
		node->update_children();
		//{
			//wxCriticalSectionLocker enter(node->children_cs);
			//con << path << " has " << node->children.size() << " children" << endl;
		//}

		App_Event* app_event = new App_Event(App_Event_IDs::nodes_added);
		//app_event->SetEventObject(this);
		wxQueueEvent(this, app_event);

		//wxQueueEvent(this, new wxCommandEvent(APP_EVENT_TYPE, (int)App_Event_IDs::nodes_added));
	}

	void OnDirPicker(wxCommandEvent& event)
	{
		// use this on a "..." button to choose a directory
		wxDirDialog dlg(null, "Choose input directory", "", wxDD_DEFAULT_STYLE);
		dlg.ShowModal();
	}

	void OnDirPickerChanged(wxFileDirPickerEvent& event)
	{
		auto path = event.GetPath();
		std::wstring path_w = path.ToStdWstring();

		WIN32_FIND_DATA data;
		HANDLE handle = FindFirstFile(path_w.data(), &data);
		if (handle == INVALID_HANDLE_VALUE)
		{
			// that doesn't exist; do nothing and return
			return;
		}

		// otherwise, the target exists
		auto node_type = get_node_type(data);

		if (node_type == Node_Type::normal_directory)
		{
			// target is a normal directory;
			// update the settings
			Settings.backup_dir.GetFullPath() = path.utf8_str();
		}
		FindClose(handle);
	}

	void OnHello(wxCommandEvent& event)
	{
		wxLogMessage("hello world from wxWidgets!");
	}

	void OnExit(wxCommandEvent& event)
	{
		Close();
	}

	void OnClose(wxCloseEvent& event)
	{
		if (event.CanVeto() && Settings.confirm_on_quit)
		{
			if (wxMessageBox(wxT("Are you sure to quit ? "), wxT("Confirm Quit"), wxICON_QUESTION | wxYES_NO) == wxYES)
			{
				// close everything
				timer->Stop();

				model->kill_threads();
				event.Skip(); //Destroy() also works here.
				Destroy();
			}
			else
				event.Veto();
		}
		else
		{
			model->kill_threads();
			Destroy();
		}
	}

	void OnAbout(wxCommandEvent& event)
	{
		wxLogMessage("About Message!");
	}
};

class MyApp : public wxApp
{
public:
	virtual bool OnInit() override
	{
		if (!wxApp::OnInit())
			return false;
		MyFrame *frame = new MyFrame("hello World", wxDefaultPosition, wxSize(450, 340));
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(MyApp);