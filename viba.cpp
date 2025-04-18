#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/richtext/richtextctrl.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <bitset>
#include <random>
#include <iomanip>

constexpr int FRAME_SIZE_BITS = 100;
constexpr uint16_t CRC_POLY = 0x1021;

class MainFrame : public wxFrame {
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, "Data Link Layer GUI", wxDefaultPosition, wxSize(1024, 768)) {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

        wxPanel* leftPanel = new wxPanel(this, wxID_ANY);
        wxPanel* rightPanel = new wxPanel(this, wxID_ANY);

        wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* senderLabel = new wxStaticText(leftPanel, wxID_ANY, "Sender");
        wxStaticText* receiverLabel = new wxStaticText(rightPanel, wxID_ANY, "Receiver");

        wxButton* loadButton = new wxButton(leftPanel, wxID_ANY, "Load .dat File");
        wxButton* pauseButton = new wxButton(leftPanel, wxID_ANY, "Pause");
        wxButton* resumeButton = new wxButton(leftPanel, wxID_ANY, "Resume");
        wxButton* cancelButton = new wxButton(leftPanel, wxID_ANY, "Cancel");

        framesCtrl = new wxTextCtrl(leftPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                     wxTE_MULTILINE | wxTE_READONLY);
        summaryCtrl = new wxRichTextCtrl(rightPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                         wxTE_MULTILINE | wxTE_READONLY);

        leftSizer->Add(senderLabel, 0, wxALL, 5);
        leftSizer->Add(loadButton, 0, wxALL | wxEXPAND, 5);
        leftSizer->Add(pauseButton, 0, wxALL | wxEXPAND, 5);
        leftSizer->Add(resumeButton, 0, wxALL | wxEXPAND, 5);
        leftSizer->Add(cancelButton, 0, wxALL | wxEXPAND, 5);
        leftSizer->Add(framesCtrl, 1, wxALL | wxEXPAND, 5);
        leftPanel->SetSizer(leftSizer);

        rightSizer->Add(receiverLabel, 0, wxALL, 5);
        rightSizer->Add(summaryCtrl, 1, wxALL | wxEXPAND, 5);
        rightPanel->SetSizer(rightSizer);

        mainSizer->Add(leftPanel, 1, wxEXPAND | wxALL, 5);
        mainSizer->Add(rightPanel, 1, wxEXPAND | wxALL, 5);
        SetSizer(mainSizer);

        Bind(wxEVT_BUTTON, &MainFrame::OnLoadFile, this, loadButton->GetId());
        pauseButton->Bind(wxEVT_BUTTON, &MainFrame::OnPause, this);
        resumeButton->Bind(wxEVT_BUTTON, &MainFrame::OnResume, this);
        cancelButton->Bind(wxEVT_BUTTON, &MainFrame::OnCancel, this);
    }

private:
    wxTextCtrl* framesCtrl;
    wxRichTextCtrl* summaryCtrl;
    bool paused = false;
    bool cancelled = false;

    void OnPause(wxCommandEvent&) { paused = true; }
    void OnResume(wxCommandEvent&) { paused = false; }
    void OnCancel(wxCommandEvent&) { cancelled = true; }

    void OnLoadFile(wxCommandEvent&) {
        wxFileDialog openFileDialog(this, "Open .dat file", "", "", "DAT files (*.dat)|*.dat",
                                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openFileDialog.ShowModal() == wxID_CANCEL) return;

        try {
            std::ifstream file(openFileDialog.GetPath().ToStdString(), std::ios::binary);
            if (!file) {
                wxMessageBox("Cannot open file.", "Error", wxICON_ERROR);
                return;
            }

            std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
            file.close();

            if (buffer.empty()) {
                wxMessageBox("File is empty.", "Error", wxICON_ERROR);
                return;
            }

            std::vector<std::string> frames = CreateFrames(buffer);
            if (frames.empty()) {
                wxMessageBox("No valid frames found in file.", "Error", wxICON_ERROR);
                return;
            }

            framesCtrl->Clear();
            summaryCtrl->Clear();

            std::stringstream ss;
            ss << "=== Frame Details ===\n";
            ss << "Total Frames: " << frames.size() << "\n\n";
            for (size_t i = 0; i < frames.size(); ++i) {
                std::string crc = CalculateCRC(frames[i]);
                ss << "Frame " << i + 1 << ": " << frames[i] << " | CRC: " << crc << "\n";
            }

            framesCtrl->SetValue(ss.str());
            std::string checksum = CalculateChecksum(frames);
            SimulateTransmission(frames, checksum);
        } catch (const std::exception& e) {
            wxMessageBox(wxString::Format("Error: %s", e.what()), "Error", wxICON_ERROR);
        }
    }

    std::vector<std::string> CreateFrames(const std::vector<char>& data) {
        std::vector<std::string> frames;
        std::string bitStream;

        for (unsigned char c : data) {
            std::bitset<8> bits(c);
            bitStream += bits.to_string();
        }

        size_t totalBits = bitStream.length();
        size_t numCompleteFrames = totalBits / FRAME_SIZE_BITS;
        size_t remainingBits = totalBits % FRAME_SIZE_BITS;

        for (size_t i = 0; i < numCompleteFrames; ++i) {
            frames.push_back(bitStream.substr(i * FRAME_SIZE_BITS, FRAME_SIZE_BITS));
        }

        if (remainingBits > 0) {
            std::string lastFrame = bitStream.substr(numCompleteFrames * FRAME_SIZE_BITS);
            lastFrame.append(FRAME_SIZE_BITS - remainingBits, '0');
            frames.push_back(lastFrame);
        }

        std::stringstream ss;
        ss << "\n=== Frame Creation Details ===\n";
        ss << "Characters in file: " << data.size() << " bytes\n";
        ss << "Total bits: " << totalBits << " bits\n";
        ss << "Bits per frame: " << FRAME_SIZE_BITS << " bits\n";
        ss << "Complete frames: " << numCompleteFrames << "\n";
        ss << "Remaining bits: " << remainingBits << "\n";
        ss << "Total frames created: " << frames.size() << "\n";
        ss << "===========================\n\n";
        framesCtrl->AppendText(ss.str());

        return frames;
    }

    std::string CalculateCRC(const std::string& frame) {
        std::string data = frame + std::string(16, '0');
        std::bitset<116> bits(data);
        std::bitset<17> poly(CRC_POLY);

        for (int i = 115; i >= 16; --i) {
            if (bits[i]) {
                for (int j = 0; j <= 16; ++j) {
                    bits[i - j] = bits[i - j] ^ poly.test(16 - j);
                }
            }
        }

        std::bitset<16> crc;
        for (int i = 0; i < 16; ++i)
            crc[i] = bits[i];

        return crc.to_string();
    }

    std::string CalculateChecksum(const std::vector<std::string>& frames) {
        uint32_t checksum = 0;

        for (const auto& frame : frames) {
            std::string crc = CalculateCRC(frame);
            uint16_t crc_value = std::bitset<16>(crc).to_ulong();
            checksum = (checksum + crc_value) & 0xFFFFFFFF;
        }

        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << checksum;
        return ss.str();
    }

    void SimulateTransmission(const std::vector<std::string>& frames, const std::string& checksum) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        size_t sentSuccess = 0, lost = 0, corrupted = 0, ackLost = 0;
        std::vector<size_t> problemFrames;

        paused = false;
        cancelled = false;

        summaryCtrl->Clear();
        summaryCtrl->WriteText("=== Transmission Summary ===\n");
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        summaryCtrl->WriteText(wxString::Format("Total Frames: %zu\n\n", frames.size()));
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());

        for (size_t i = 0; i < frames.size(); ++i) {
            if (cancelled) break;
            while (paused) {
                wxMilliSleep(100);
                wxYield();
            }

            double prob = dis(gen);
            wxString status;
            wxColour color;

            if (prob < 0.10) {
                lost++;
                status = "LOST";
                color = *wxRED;
                problemFrames.push_back(i + 1);
            } else if (prob < 0.30) {
                corrupted++;
                status = "CORRUPTED";
                color = wxColour(128, 0, 128);
                problemFrames.push_back(i + 1);
            } else if (prob < 0.45) {
                ackLost++;
                status = "ACK LOST";
                color = *wxBLUE;
                problemFrames.push_back(i + 1);
            } else {
                sentSuccess++;
                status = "OK";
                color = *wxGREEN;
            }

            summaryCtrl->BeginTextColour(color);
            summaryCtrl->WriteText(wxString::Format("Frame %03zu: %s\n", i + 1, status));
            summaryCtrl->EndTextColour();
            summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());

            wxMilliSleep(5);
            wxYield();
        }

        bool checksumCorrupted = (dis(gen) < 0.05);

        summaryCtrl->WriteText("\n--- Transmission Stats ---\n");
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        summaryCtrl->WriteText(wxString::Format("Successfully Transmitted: %zu frames\n", sentSuccess));
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        summaryCtrl->WriteText(wxString::Format("Lost Frames: %zu (%.2f%%)\n", lost, (lost * 100.0 / frames.size())));
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        summaryCtrl->WriteText(wxString::Format("Corrupted Frames: %zu (%.2f%%)\n", corrupted, (corrupted * 100.0 / frames.size())));
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        summaryCtrl->WriteText(wxString::Format("ACK Lost: %zu (%.2f%%)\n", ackLost, (ackLost * 100.0 / frames.size())));
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());

        if (!problemFrames.empty()) {
            summaryCtrl->WriteText("\nProblem Frames: ");
            for (size_t i = 0; i < problemFrames.size(); ++i) {
                summaryCtrl->WriteText(wxString::Format("%zu", problemFrames[i]));
                if (i < problemFrames.size() - 1) summaryCtrl->WriteText(", ");
            }
            summaryCtrl->WriteText("\n");
            summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
        }

        summaryCtrl->WriteText("\nChecksum Status: ");
        summaryCtrl->WriteText(checksumCorrupted ? "CORRUPTED\n" : "Successfully Transmitted\n");
        summaryCtrl->WriteText(wxString::Format("Checksum Value: %s\n", checksum));
        summaryCtrl->WriteText("===========================\n");
        summaryCtrl->ShowPosition(summaryCtrl->GetLastPosition());
    }
};

class MyApp : public wxApp {
public:
    virtual bool OnInit() {
        MainFrame* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);
