#include "filezilla.h"
#include "storj_key_interface.h"
#include "Options.h"
#include "filezillaapp.h"
#include "inputdialog.h"

#include <libfilezilla/local_filesys.hpp>
#include <libfilezilla/process.hpp>

CStorjKeyInterface::CStorjKeyInterface(COptionsBase & options, wxWindow* parent)
	: options_(options)
	, m_parent(parent)
{
}

CStorjKeyInterface::~CStorjKeyInterface()
{
}

namespace {
std::wstring quote(std::wstring_view const& arg)
{
	return L"\"" + fz::replaced_substrings(arg, L"\"", L"\"\"") + L"\"";
}
}
std::tuple<bool, std::wstring> CStorjKeyInterface::ValidateGrant(std::wstring const& grant, bool silent)
{
	if (grant.empty()) {
		return {false, std::wstring()};
	}
	LoadProcess(silent);

	if (!Send(L"validate grant " + quote(grant))) {
		return {false, std::wstring()};
	}

	std::wstring reply;
	ReplyCode code = GetReply(reply);
	return {code == success, reply};
}

bool CStorjKeyInterface::LoadProcess(bool silent)
{
	if (m_initialized) {
		return m_process != 0;
	}
	m_initialized = true;

	std::wstring executable = options_.get_string(OPTION_FZSTORJ_EXECUTABLE);
	size_t pos = executable.rfind(fz::local_filesys::path_separator);
	if (pos == std::wstring::npos) {
		if (!silent) {
			wxMessageBoxEx(_("fzstorj could not be started.\nPlease make sure this executable exists in the same directory as the main FileZilla executable."), _("Error starting program"), wxICON_EXCLAMATION);
		}
		return false;
	}

	executable = executable.substr(0, pos + 1) + L"fzstorj";
#ifdef FZ_WINDOWS
	executable += L".exe";
#endif

	m_process = std::make_unique<fz::process>();
	if (!m_process->spawn(fz::to_native(executable))) {
		m_process.reset();

		if (!silent) {
			wxMessageBoxEx(_("fzstorj could not be started.\nPlease make sure this executable exists in the same directory as the main FileZilla executable."), _("Error starting program"), wxICON_EXCLAMATION);
		}
		return false;
	}

	return true;
}

bool CStorjKeyInterface::Send(std::wstring const& cmd)
{
	if (!m_process) {
		return false;
	}

	std::string utf8 = fz::to_utf8(cmd) + "\n";
	if (!m_process->write(utf8)) {
		m_process.reset();

		wxMessageBoxEx(_("Could not send command to fzstorj."), _("Command failed"), wxICON_EXCLAMATION);
		return false;
	}

	return true;
}

CStorjKeyInterface::ReplyCode CStorjKeyInterface::GetReply(std::wstring & reply)
{
	if (!m_process) {
		return error;
	}

	char buffer[100];

	std::string input;

	for (;;) {
		size_t pos = input.find_first_of("\r\n");
		if (pos == std::string::npos) {
			int read = m_process->read(buffer, 100);
			if (read <= 0) {
				wxMessageBoxEx(_("Could not get reply from fzstorj."), _("Command failed"), wxICON_EXCLAMATION);
				m_process.reset();
				return error;
			}

			input.append(buffer, read);
			continue;
		}

		// Strip everything behind first linebreak.
		if (!pos) {
			input = input.substr(1);
			continue;
		}

		char c = input[0];

		reply = fz::to_wstring_from_utf8(input.substr(1, pos - 1));
		input = input.substr(pos + 1);

		if (c == '1') {
			return success;
		}
		else if (c == '2') {
			return error;
		}
		// Ignore others
	}
}

bool CStorjKeyInterface::ProcessFailed() const
{
	return m_initialized && !m_process;
}
