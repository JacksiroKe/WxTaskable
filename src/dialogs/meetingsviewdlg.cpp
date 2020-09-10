// Productivity tool to help you track the time you spend on tasks
// Copyright (C) 2020  Szymon Welgus
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
//  Contact:
//    szymonwelgus at gmail dot com

#include "meetingsviewdlg.h"

#include <functional>

#include <wx/statline.h>

#include "../common/common.h"
#include "../data/meetingdata.h"
#include "../dialogs/taskitemdlg.h"

wxDEFINE_EVENT(GET_MEETINGS_THREAD_COMPLETED, wxThreadEvent);
wxDEFINE_EVENT(GET_MEETINGS_THREAD_ERROR, wxThreadEvent);

namespace app::dlg
{
GetMeetingsThread::GetMeetingsThread(MeetingsViewDialog* handler)
    : wxThread(wxTHREAD_DETACHED)
    , pHandler(handler)
{
}

GetMeetingsThread::~GetMeetingsThread()
{
    wxCriticalSectionLocker enter(pHandler->mCriticalSection);
    pHandler->pThread = nullptr;
}

wxThread::ExitCode GetMeetingsThread::Entry()
{
    wxOleInitialize();
    svc::OutlookIntegrator outlookIntegrator;

    wxString eventMessage = wxEmptyString;
    if (!outlookIntegrator.TryGetOutlookInstance()) {
        eventMessage = wxT("Could not connect to Outlook!");
        auto event = new wxThreadEvent(GET_MEETINGS_THREAD_ERROR);
        event->SetString(eventMessage);
        wxQueueEvent(pHandler, event);
        return (wxThread::ExitCode) 1;
    }

    if (!outlookIntegrator.Execute()) {
        eventMessage = wxT("An unexpected error occured while getting meetings from Outlook!");
        auto event = new wxThreadEvent(GET_MEETINGS_THREAD_ERROR);
        event->SetString(eventMessage);
        wxQueueEvent(pHandler, event);
        return (wxThread::ExitCode) 1;
    }

    auto event = new wxThreadEvent(GET_MEETINGS_THREAD_COMPLETED);
    wxQueueEvent(pHandler, event);

    pHandler->CallAfter(
        std::bind(&MeetingsViewDialog::GetMeetingDataFromThread, pHandler, outlookIntegrator.GetMeetings()));

    return (wxThread::ExitCode) 0;
}

void GetMeetingsThread::OnExit()
{
    wxOleUninitialize();
    wxThread::OnExit();
}

MeetingsViewDialog::MeetingsViewDialog(wxWindow* parent,
    std::shared_ptr<spdlog::logger> logger,
    std::shared_ptr<cfg::Configuration> config,
    const wxString& name)
    : pThread(nullptr)
    , mCriticalSection()
    , pLogger(logger)
    , pConfig(config)
    , pScrolledWindow(nullptr)
    , pTodayDateLabel(nullptr)
    , pActivityIndicator(nullptr)
{
    Create(parent, wxID_ANY, wxT("Meetings View"), wxDefaultPosition, wxSize(400, 700), wxCAPTION | wxCLOSE_BOX, name);
}

MeetingsViewDialog::~MeetingsViewDialog()
{
    while (!mMeetings.empty()) {
        delete mMeetings.back();
        mMeetings.pop_back();
    }
}

void MeetingsViewDialog::LaunchModeless()
{
    if (IsElevated()) {
        pActivityIndicator->Hide();
        wxString runAsAdminErrorText =
            wxT("Error! Cannot read meetings from Outlook\nwhen host process is run as administrator.");
        auto runAsAdminErrorLabel = new wxStaticText(pScrolledWindow, wxID_ANY, runAsAdminErrorText);
        runAsAdminErrorLabel->SetFont(wxFont(wxNORMAL_FONT->GetPointSize(),
            wxFONTFAMILY_DEFAULT,
            wxFONTSTYLE_ITALIC,
            wxFONTWEIGHT_NORMAL,
            false,
            wxEmptyString));
        runAsAdminErrorLabel->Wrap(-1);
        pScrolledWindow->GetSizer()->Add(runAsAdminErrorLabel, wxSizerFlags().Center());
        pScrolledWindow->GetSizer()->Layout();
    } else {
        pActivityIndicator->Start();
        StartThread();
    }

    wxDialog::Show(true);
}

void MeetingsViewDialog::GetMeetingDataFromThread(std::vector<svc::Meeting*> meetings)
{
    if (meetings.empty()) {
        wxString noMeetingsText = wxT("No meetings found...");
        auto noMeetingsLabel = new wxStaticText(pScrolledWindow, wxID_ANY, noMeetingsText);
        noMeetingsLabel->SetFont(wxFont(wxNORMAL_FONT->GetPointSize(),
            wxFONTFAMILY_DEFAULT,
            wxFONTSTYLE_ITALIC,
            wxFONTWEIGHT_NORMAL,
            false,
            wxEmptyString));
        noMeetingsLabel->Wrap(-1);
        pScrolledWindow->GetSizer()->Add(noMeetingsLabel, wxSizerFlags().Center());
    } else {
        for (auto meeting : meetings) {
            AppendMeetingControls(meeting);
        }

        mMeetings = meetings;
    }
    pScrolledWindow->GetSizer()->Layout();
}

bool MeetingsViewDialog::Create(wxWindow* parent,
    wxWindowID windowId,
    const wxString& title,
    const wxPoint& position,
    const wxSize& size,
    long style,
    const wxString& name)
{
    SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
    bool created = wxDialog::Create(parent, windowId, title, position, size, style, name);
    if (created) {
        CreateControls();
        ConfigureEventBindings();
        FillControls();

        // GetSizer()->Fit(this);
        // GetSizer()->SetSizeHints(this);
        SetIcon(common::GetProgramIcon());
        Center();
    }

    return created;
}

void MeetingsViewDialog::CreateControls()
{
    /* Window Sizing */
    auto mainSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(mainSizer);

    /* Main Scrolled Window */
    pScrolledWindow = new wxScrolledWindow(this, wxID_ANY);
    mainSizer->Add(pScrolledWindow, wxSizerFlags(1).Expand());

    auto scrolledSizer = new wxBoxSizer(wxVERTICAL);
    pScrolledWindow->SetSizer(scrolledSizer);

    /* Today Date Label */
    pTodayDateLabel = new wxStaticText(pScrolledWindow, IDC_TODAYDATE, wxGetEmptyString());
    auto todayDateLabelFont = pTodayDateLabel->GetFont();
    todayDateLabelFont.SetPointSize(14);
    pTodayDateLabel->SetFont(todayDateLabelFont);
    scrolledSizer->Add(pTodayDateLabel, wxSizerFlags().CenterHorizontal().Border(wxALL, 5).Top());

    /* Activity Indicator */
    pActivityIndicator = new wxActivityIndicator(pScrolledWindow, IDC_ACTIVITYINDICATOR);
    scrolledSizer->Add(pActivityIndicator, wxSizerFlags(1).Center());
}

// clang-format off
void MeetingsViewDialog::ConfigureEventBindings()
{
    Bind(
        wxEVT_CLOSE_WINDOW,
        &MeetingsViewDialog::OnClose,
        this
    );

    Bind(
        GET_MEETINGS_THREAD_COMPLETED,
        &MeetingsViewDialog::OnThreadCompletion,
        this
    );

    Bind(
        GET_MEETINGS_THREAD_ERROR,
        &MeetingsViewDialog::OnThreadError,
        this
    );
}
// clang-format on

void MeetingsViewDialog::FillControls()
{
    auto today = wxDateTime::Now();
    pTodayDateLabel->SetLabel(today.FormatISODate());
}

void MeetingsViewDialog::AppendMeetingControls(svc::Meeting* meeting)
{
    auto meetingStaticBox = new wxStaticBox(pScrolledWindow, wxID_ANY, wxGetEmptyString());
    auto meetingStaticBoxSizer = new wxStaticBoxSizer(meetingStaticBox, wxVERTICAL);
    pScrolledWindow->GetSizer()->Add(meetingStaticBoxSizer, common::sizers::ControlExpand);

    auto subjectLabel = new wxStaticText(meetingStaticBox, wxID_ANY, meeting->Subject);
    meetingStaticBoxSizer->Add(subjectLabel, common::sizers::ControlDefault);

    /* Horizontal Line*/
    auto separation_line =
        new wxStaticLine(meetingStaticBox, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
    meetingStaticBoxSizer->Add(separation_line, 0, wxEXPAND | wxALL, 1);

    auto locationLabel = new wxStaticText(meetingStaticBox, wxID_ANY, meeting->Location);
    locationLabel->SetLabelMarkup(meeting->Location);
    locationLabel->Wrap(-1);
    locationLabel->SetFont(
        wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL, false, wxEmptyString));
    locationLabel->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
    meetingStaticBoxSizer->Add(locationLabel, common::sizers::ControlDefault);

    auto startLabel = new wxStaticText(
        meetingStaticBox, wxID_ANY, wxString::Format(wxT("  Start: %s"), meeting->Start.FormatISOCombined(' ')));
    startLabel->Wrap(-1);
    meetingStaticBoxSizer->Add(startLabel, common::sizers::ControlDefault);

    auto endLabel = new wxStaticText(
        meetingStaticBox, wxID_ANY, wxString::Format(wxT("  End: %s"), meeting->End.FormatISOCombined(' ')));
    endLabel->Wrap(-1);
    meetingStaticBoxSizer->Add(endLabel, common::sizers::ControlDefault);

    auto attendedCheckbox = new wxCheckBox(meetingStaticBox, wxID_ANY, wxT("Attended"));
    wxWindowID attendedCheckboxControlId = attendedCheckbox->GetId();
    meeting->Identifier = attendedCheckboxControlId;
    attendedCheckbox->Bind(
        wxEVT_CHECKBOX, &MeetingsViewDialog::OnAttendedCheckboxCheck, this, attendedCheckboxControlId);
    meetingStaticBoxSizer->Add(attendedCheckbox, common::sizers::ControlDefault);
}

void MeetingsViewDialog::StartThread()
{
    pThread = new GetMeetingsThread(this);
    auto ret = pThread->Run();
    if (ret != wxTHREAD_NO_ERROR) {
        delete pThread;
        pThread = nullptr;
    }
}

void MeetingsViewDialog::ThreadCleanupProcedure()
{
    {
        wxCriticalSectionLocker enter(mCriticalSection);
        if (pThread) {
            auto ret = pThread->Delete();
            if (ret != wxTHREAD_NO_ERROR) {
                wxLogError("Cannot delete thread!");
            }
        }
    }

    while (1) {
        {
            wxCriticalSectionLocker enter(mCriticalSection);
            if (!pThread) {
                break;
            }
        }
        wxThread::This()->Sleep(1);
    }

    Destroy();
}

void MeetingsViewDialog::OnClose(wxCloseEvent& event)
{
    ThreadCleanupProcedure();
}

void MeetingsViewDialog::OnThreadCompletion(wxThreadEvent& event)
{
    pActivityIndicator->Stop();
    pActivityIndicator->Hide();
}

void MeetingsViewDialog::OnThreadError(wxThreadEvent& event)
{
    pActivityIndicator->Stop();
    pActivityIndicator->Hide();

    wxString errorOccuredText = wxT("Error occured: %s");
    auto errorOccuredLabel =
        new wxStaticText(pScrolledWindow, wxID_ANY, wxString::Format(errorOccuredText, event.GetString()));
    errorOccuredLabel->Wrap(-1);

    pScrolledWindow->GetSizer()->AddSpacer(64);
    pScrolledWindow->GetSizer()->Add(errorOccuredLabel, wxSizerFlags(1).Center());
    pScrolledWindow->GetSizer()->Layout();
}

void MeetingsViewDialog::OnAttendedCheckboxCheck(wxCommandEvent& event)
{
    auto iterator = std::find_if(mMeetings.begin(), mMeetings.end(), [&](svc::Meeting* meeting) {
        return meeting->Identifier == static_cast<wxWindowID>(event.GetId());
    });

    if (iterator != mMeetings.end()) {
        auto meeting = *iterator;

        dlg::TaskItemDialog taskItemMeetingDialog(
            this->GetParent(), pLogger, pConfig, constants::TaskItemTypes::TimedTask);
        taskItemMeetingDialog.SetMeetingData(meeting);
        int retCode = taskItemMeetingDialog.ShowModal();

        auto selectedCheckbox = (wxCheckBox*) wxWindow::FindWindowById(event.GetId());

        if (retCode == wxID_OK) {
            data::MeetingData meetingData;
            selectedCheckbox->Disable();

            auto meetingModel = std::make_unique<model::MeetingModel>();
            auto meetingAttended = std::make_unique<bool>(selectedCheckbox->GetValue());
            meetingModel->Attended(std::move(meetingAttended));
            meetingModel->SetBody(meeting->Body);
            meetingModel->SetDuration(meeting->Duration);
            meetingModel->SetLocation(meeting->Location);
            meetingModel->SetSubject(meeting->Subject);
            meetingModel->SetStart(meeting->Start);
            meetingModel->SetEnd(meeting->End);

            meetingModel->SetTaskItemId(taskItemMeetingDialog.GetTaskItemId());

            try {
                meetingData.Create(std::move(meetingModel));
            } catch (const sqlite::sqlite_exception& e) {
                pLogger->error("Error occured in MeetingData::Create() - {0:d} : {1}", e.get_code(), e.what());
                wxLogDebug(wxString(e.get_sql()));
            }
        } else {
            selectedCheckbox->SetValue(false);
        }
    }
}
} // namespace app::dlg