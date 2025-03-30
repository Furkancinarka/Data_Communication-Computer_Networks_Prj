#include <wx/wx.h>
#include <wx/filedlg.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <bitset>
#include <random>

constexpr int FRAME_SIZE_BITS = 100;
constexpr int FRAME_SIZE_BYTES = FRAME_SIZE_BITS / 8;
constexpr uint16_t CRC_POLY = 0x1021; // x^16 + x^12 + x^5 + 1

class MainFrame : public wxFrame {
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, "Data Link Layer GUI", wxDefaultPosition, wxSize(1024, 768)) {
        wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);
        
        // Create panels with visible borders
        wxPanel* leftPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
        wxPanel* rightPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
        
        // Create sizers for panels
        wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

        // Left panel - Frame details
        wxButton* loadButton = new wxButton(leftPanel, wxID_ANY, "Load .dat File");
        framesCtrl = new wxTextCtrl(leftPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY);

        leftSizer->Add(loadButton, 0, wxALL | wxEXPAND, 5);
        leftSizer->Add(framesCtrl, 1, wxALL | wxEXPAND, 5);
        leftPanel->SetSizer(leftSizer);

        // Right panel - Transmission summary
        wxStaticText* summaryLabel = new wxStaticText(rightPanel, wxID_ANY, "Transmission Summary");
        summaryCtrl = new wxTextCtrl(rightPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                   wxTE_MULTILINE | wxTE_READONLY);
        
        rightSizer->Add(summaryLabel, 0, wxALL, 5);
        rightSizer->Add(summaryCtrl, 1, wxALL | wxEXPAND, 5);
        rightPanel->SetSizer(rightSizer);
        rightPanel->Layout();

        // Add panels to main sizer with equal width
        mainSizer->Add(leftPanel, 1, wxEXPAND | wxALL, 5);
        mainSizer->Add(rightPanel, 1, wxEXPAND | wxALL, 5);
        
        SetSizer(mainSizer);

        // Bind events
        Bind(wxEVT_BUTTON, &MainFrame::OnLoadFile, this, loadButton->GetId());
    }

private:
    wxTextCtrl* framesCtrl;  // For frame details
    wxTextCtrl* summaryCtrl; // For transmission summary
    
    struct FrameState {
        std::string data;
        std::string crc;
        enum Status { WAITING, SENDING, SENT, LOST, CORRUPTED, ACK_LOST } status;
        int position;
    };
    std::vector<FrameState> frameStates;
    size_t currentFrame = 0;

    void OnLoadFile(wxCommandEvent&) {
        wxFileDialog openFileDialog(this, "Open .dat file", "", "",
                                     "DAT files (*.dat)|*.dat", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

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

            // Clear both panels
            framesCtrl->Clear();
            summaryCtrl->Clear();

            // Show frame details in left panel
            std::stringstream ss;
            ss << "=== Frame Details ===\n";
            ss << "Total Frames: " << frames.size() << "\n\n";
            
            for (size_t i = 0; i < frames.size(); ++i) {
                try {
                    std::string crc = CalculateCRC(frames[i]);
                    ss << "Frame " << i + 1 << ": " << frames[i] << " | CRC: " << crc << "\n";
                } catch (const std::exception& e) {
                    wxMessageBox(wxString::Format("Error processing frame %zu: %s", i + 1, e.what()),
                               "Error", wxICON_ERROR);
                    return;
                }
            }

            framesCtrl->SetValue(ss.str());  // Use left panel for frames

            try {
                std::string checksum = CalculateChecksum(frames);
                SimulateTransmission(frames, checksum);
            } catch (const std::exception& e) {
                wxMessageBox("Error calculating checksum", "Error", wxICON_ERROR);
                return;
            }
        }
        catch (const std::exception& e) {
            wxMessageBox(wxString::Format("Error: %s", e.what()), "Error", wxICON_ERROR);
        }
    }

    std::vector<std::string> CreateFrames(const std::vector<char>& data) {
        std::vector<std::string> frames;
        std::string bitStream;
        
        // Convert ALL characters to bits (including whitespace, punctuation, etc.)
        for (unsigned char c : data) {
            std::bitset<8> bits(c);
            bitStream += bits.to_string();
        }

        // Calculate total frames needed
        const size_t totalBits = bitStream.length();
        const size_t frameSizeBits = static_cast<size_t>(FRAME_SIZE_BITS);
        const size_t numCompleteFrames = totalBits / frameSizeBits;
        const size_t remainingBits = totalBits % frameSizeBits;
        
        frames.reserve(numCompleteFrames + (remainingBits > 0 ? 1 : 0));

        // Split into complete 100-bit frames
        for (size_t i = 0; i < numCompleteFrames; ++i) {
            frames.push_back(bitStream.substr(i * frameSizeBits, frameSizeBits));
        }

        // Handle remaining bits if any
        if (remainingBits > 0) {
            std::string lastFrame = bitStream.substr(numCompleteFrames * frameSizeBits);
            lastFrame.append(frameSizeBits - remainingBits, '0'); // Pad with zeros
            frames.push_back(lastFrame);
        }

        // Detailed debug output
        std::stringstream ss;
        ss << "\n=== Frame Creation Details ===\n";
        ss << "Characters in file: " << data.size() << " bytes\n";
        ss << "Total bits: " << totalBits << " bits\n";
        ss << "Bits per frame: " << frameSizeBits << " bits\n";
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
                    bits[i - j] = bits[i - j] ^ poly[16 - j];
                }
            }
        }

        std::bitset<16> crc;
        for (int i = 0; i < 16; ++i) crc[i] = bits[i];
        return crc.to_string();
    }

    std::string CalculateChecksum(const std::vector<std::string>& frames) {
        if (frames.empty()) {
            throw std::runtime_error("No frames to calculate checksum");
        }

        // Her frame'in CRC'sini hesapla ve topla
        uint32_t checksum = 0;
        for (const auto& frame : frames) {
            // Frame'in CRC'sini al
            std::string crc = CalculateCRC(frame);
            
            // CRC'yi 16-bitlik unsigned integer'a çevir
            uint16_t crc_value = std::bitset<16>(crc).to_ulong();
            
            // Checksum'a ekle
            checksum = (checksum + crc_value) & 0xFFFFFFFF;
        }

        // Hexadecimal formatında döndür
        std::stringstream ss;
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << checksum;
        return ss.str();
    }

    void SimulateTransmission(const std::vector<std::string>& frames, const std::string& checksum) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        size_t sentSuccess = 0;
        size_t lost = 0;
        size_t corrupted = 0;
        size_t ackLost = 0;
        std::vector<size_t> problemFrames;

        // Simulate transmission for each frame
        for (size_t i = 0; i < frames.size(); ++i) {
            double prob = dis(gen);
            if (prob < 0.10) {
                lost++;
                problemFrames.push_back(i + 1);
            }
            else if (prob < 0.30) {
                corrupted++;
                problemFrames.push_back(i + 1);
            }
            else if (prob < 0.45) {
                ackLost++;
                problemFrames.push_back(i + 1);
            }
            else {
                sentSuccess++;
            }
        }

        // Simulate checksum transmission
        bool checksumCorrupted = (dis(gen) < 0.05);

        // Generate transmission report
        std::stringstream ss;
        ss << "=== Transmission Summary ===\n";
        ss << "Total Frames: " << frames.size() << "\n";
        ss << "Successfully Transmitted: " << sentSuccess << " frames\n";
        ss << "Lost Frames: " << lost << " (" << (lost * 100.0 / frames.size()) << "%)\n";
        ss << "Corrupted Frames: " << corrupted << " (" << (corrupted * 100.0 / frames.size()) << "%)\n";
        ss << "ACK Lost: " << ackLost << " (" << (ackLost * 100.0 / frames.size()) << "%)\n";
        
        if (!problemFrames.empty()) {
            ss << "\nProblem Frames: ";
            for (size_t i = 0; i < problemFrames.size(); ++i) {
                ss << problemFrames[i];
                if (i < problemFrames.size() - 1) ss << ", ";
            }
            ss << "\n";
        }

        ss << "\nChecksum Status: ";
        if (checksumCorrupted) {
            ss << "CORRUPTED\n";
        } else {
            ss << "Successfully Transmitted\n";
        }
        
        ss << "Checksum Value: " << checksum << "\n";
        ss << "===========================\n\n";

        summaryCtrl->SetValue(ss.str());  // Use right panel for summary
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
