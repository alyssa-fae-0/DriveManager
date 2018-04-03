#pragma once
#include "stdafx.h"
#include "console.h"

enum struct App_event_id  : int
{
	nodes_added,
	thread_node_size_calc_finished,
	thread_node_size_calc_failed,
	num_active_threads_update
};

struct Fs_node;

class App_event;
wxDECLARE_EVENT(App_event_type, App_event);

class App_event : public wxCommandEvent
{
public:
	Fs_node* node;

	App_event(App_event_id id)
		: wxCommandEvent(App_event_type, (int)id)
	{
		//con << "Event created: " << endl << "  Event type: " << commandType << endl << "  id: " << id << endl;
		node = null;
	}

	// You *must* copy here the data to be transported
	App_event(const App_event& event)
		: wxCommandEvent(event) 
	{
		//con << "Event copy: " << endl << "  Event type: " << this->GetEventType() << endl << "  id: " << event.GetId() << endl;
		this->node = event.node;
	}

	// Required for sending with wxPostEvent()
	wxEvent* Clone() const { return new App_event(*this); }	
};

#define App_Event_Handler(func) (&func)

//typedef void (wxEvtHandler::*App_Event_Function)(App_Event &);
//
//#define App_Event_Handler(func) wxEVENT_HANDLER_CAST(App_Event_Function, func)




//bwxDEFINE_EVENT(APP_EVENT, )

// define a new custom event type, can be used alone or after event
// declaration in the header using one of the macros below
//#define wxDEFINE_EVENT( name, type ) \
//    const wxEventTypeTag< type > name( wxNewEventType() )

//class App_Event : public wxEvent
//{
//public:
//	App_Event(int id)
//	{
//		this->m_id = id;
//		wxEventType type;
//	}
//	//wxEvent(int winid = 0, wxEventType commandType = wxEVT_NULL);
//
//	// This function is used to create a copy of the event polymorphically and
//	// all derived classes must implement it because otherwise wxPostEvent()
//	// for them wouldn't work (it needs to do a copy of the event)
//	virtual wxEvent *Clone() const = 0
//	{
//
//	}
//
//	// this function is used to selectively process events in wxEventLoopBase::YieldFor
//	// NOTE: by default it returns wxEVT_CATEGORY_UI just because the major
//	//       part of wxWidgets events belong to that category.
//	virtual wxEventCategory GetEventCategory() const override
//	{
//		return wxEventCategory::wxEVT_CATEGORY_UNKNOWN;
//	}
//};