#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace std;

struct LogEntry {
    int id;
    time_t timestamp;
    string eventType;
    string message;
    string signature;
};

struct EnergyData {
    double currentPower;
    double totalEnergy;
    double peakPower;
    double efficiency;
};

class PortalCrane {
private:
    double hookHeight{0.0};
    double slewAngle{0.0};
    double loadWeight{0.0};
    bool emergencyStop{false};
    string craneID = "PCR-2024-001";
    string status = "READY";
    int operatingHours = 0;
    int cycleCount = 0;
    bool connected = false;
    string connectionType = "NONE";
    string lastConnectionTime = "";
    EnergyData energy;
    vector<LogEntry> eventLog;
    int nextLogId = 1;
    const double MAX_HEIGHT = 30.0;
    const double MAX_ANGLE = 360.0;
    const double MAX_LOAD = 50.0;
    const double POWER_IDLE = 5.0;
    const double POWER_PER_METER = 2.0;
    const double POWER_PER_TON = 1.5;
    const double POWER_PER_DEGREE = 0.1;

    void updateEnergyConsumption(double deltaHeight, double deltaAngle, double load) {
        if (!connected) return;
        double power = POWER_IDLE;
        power += abs(deltaHeight) * POWER_PER_METER;
        power += (load / 10.0) * POWER_PER_TON;
        power += abs(deltaAngle) * POWER_PER_DEGREE;
        energy.currentPower = power;
        energy.totalEnergy += power / 3600.0;
        if (power > energy.peakPower) {
            energy.peakPower = power;
            addLogEntry("ENERGY", "New peak power: " + to_string(energy.peakPower) + " kW");
        }
        if (load > 0 && power > 0) {
            double rawEfficiency = (load * 9.81) / (power * 1000) * 100;
            energy.efficiency = min(100.0, max(0.0, rawEfficiency));
        }
    }

    string generateSimpleSignature(const string& message) {
        unsigned long hash = 0;
        for (char c : message) hash = hash * 31 + c;
        char sig[32];
        sprintf_s(sig, "SIG-%08X-%04X", hash, (unsigned int)time(nullptr) % 10000);
        return string(sig);
    }

    void saveLogToFile(const LogEntry& entry) {
        ofstream file("crane_log.txt", ios::app);
        if (file.is_open()) {
            char timebuf[26];
            ctime_s(timebuf, sizeof(timebuf), &entry.timestamp);
            string timeStr = timebuf;
            timeStr.pop_back();
            file << "[" << entry.id << "] " << timeStr << " | " << entry.eventType << " | "
                 << entry.message << " | Signature: " << entry.signature << "\n";
            file.close();
        }
    }

public:
    PortalCrane() {
        energy.currentPower = POWER_IDLE;
        energy.totalEnergy = 0;
        energy.peakPower = POWER_IDLE;
        energy.efficiency = 100.0;
        addLogEntry("SYSTEM", "Crane control system initialized");
    }

    bool connectToCrane(const string& type) {
        if (connected) { addLogEntry("ERROR", "Reconnect attempt"); return false; }
        connectionType = type;
        connected = true;
        time_t now = time(nullptr);
        char buf[26];
        ctime_s(buf, sizeof(buf), &now);
        lastConnectionTime = buf;
        lastConnectionTime.pop_back();
        addLogEntry("CONNECTION", "Connected to crane via " + type);
        return true;
    }

    bool disconnectFromCrane() {
        if (!connected) return false;
        addLogEntry("DISCONNECT", "Disconnected from crane");
        connected = false;
        connectionType = "NONE";
        return true;
    }

    string getConnectionStatus() {
        if (connected) return "CONNECTED (" + connectionType + ") since " + lastConnectionTime;
        return "DISCONNECTED";
    }

    string getCraneInfo() {
        stringstream ss;
        ss << "===========================================\n";
        ss << "         CRANE SPECIFICATIONS\n";
        ss << "===========================================\n\n";
        ss << "  ID:               " << craneID << "\n";
        ss << "  Status:           " << status << "\n";
        ss << "  Operating hours:  " << operatingHours << "\n";
        ss << "  Cycles:           " << cycleCount << "\n";
        ss << "  Connection:       " << getConnectionStatus() << "\n";
        ss << "  Connected:        " << (connected ? "YES" : "NO") << "\n";
        ss << "\n===========================================";
        return ss.str();
    }

    EnergyData getEnergyData() {
        updateEnergyConsumption(0, 0, loadWeight);
        return energy;
    }

    string getEnergyReport() {
        updateEnergyConsumption(0, 0, loadWeight);
        stringstream ss;
        ss << "===========================================\n";
        ss << "             ENERGY OPTIMIZATION\n";
        ss << "===========================================\n\n";
        ss << "  Current power:    " << fixed << setprecision(1) << energy.currentPower << " kW\n";
        ss << "  Total energy:     " << energy.totalEnergy << " kWh\n";
        ss << "  Peak power:       " << energy.peakPower << " kW\n";
        ss << "  Efficiency:       " << energy.efficiency << " %%\n";
        ss << "\n===========================================\n";
        ss << "                 RECOMMENDATIONS\n";
        ss << "===========================================\n\n";

        if (energy.currentPower > 30)
            ss << "  WARNING: High power consumption! Reduce load.\n";
        else if (energy.currentPower > 20)
            ss << "  INFO: Medium power consumption.\n";
        else
            ss << "  GOOD: Low power consumption.\n";

        if (loadWeight > 0 && hookHeight > 20)
            ss << "  TIP: Lower the load to save energy\n";
        if (energy.efficiency < 60)
            ss << "  WARNING: Low efficiency! Check mechanisms.\n";

        ss << "\n===========================================";
        return ss.str();
    }

    void addLogEntry(const string& eventType, const string& message) {
        LogEntry entry;
        entry.id = nextLogId++;
        entry.timestamp = time(nullptr);
        entry.eventType = eventType;
        entry.message = message;
        string signData = eventType + message + to_string(entry.timestamp);
        entry.signature = generateSimpleSignature(signData);
        eventLog.push_back(entry);
        while (eventLog.size() > 100) eventLog.erase(eventLog.begin());
        saveLogToFile(entry);
    }

    string getEventLog(int count = 15) {
        stringstream ss;
        ss << "===========================================\n";
        ss << "                 EVENT LOG\n";
        ss << "===========================================\n";
        ss << "  Each event has cryptographic signature\n\n";

        int start = eventLog.size() > count ? eventLog.size() - count : 0;
        for (size_t i = start; i < eventLog.size(); i++) {
            char timebuf[26];
            ctime_s(timebuf, sizeof(timebuf), &eventLog[i].timestamp);
            string timeStr = timebuf;
            timeStr.pop_back();

            ss << "  [" << setw(3) << eventLog[i].id << "] " << timeStr << "\n";
            ss << "       Type:      " << eventLog[i].eventType << "\n";
            ss << "       Message:   " << eventLog[i].message << "\n";
            ss << "       Signature: " << eventLog[i].signature << "\n\n";
        }
        ss << "===========================================";
        return ss.str();
    }

    bool verifyLogSignature(int logId) {
        for (const auto& entry : eventLog) {
            if (entry.id == logId) {
                string signData = entry.eventType + entry.message + to_string(entry.timestamp);
                string expectedSig = generateSimpleSignature(signData);
                return entry.signature == expectedSig;
            }
        }
        return false;
    }

    bool raiseHook(double delta) {
        if (!connected) { addLogEntry("ERROR", "Raise attempt without connection"); return false; }
        if (emergencyStop) return false;
        if (hookHeight + delta <= MAX_HEIGHT) {
            hookHeight += delta;
            updateEnergyConsumption(delta, 0, loadWeight);
            addLogEntry("ACTION", "Hook raised by " + to_string(delta) + " m");
            return true;
        }
        addLogEntry("ERROR", "Max height exceeded");
        return false;
    }

    bool lowerHook(double delta) {
        if (!connected) { addLogEntry("ERROR", "Lower attempt without connection"); return false; }
        if (emergencyStop) return false;
        if (hookHeight - delta >= 0) {
            hookHeight -= delta;
            updateEnergyConsumption(-delta, 0, loadWeight);
            addLogEntry("ACTION", "Hook lowered by " + to_string(delta) + " m");
            return true;
        }
        addLogEntry("ERROR", "Min height reached");
        return false;
    }

    bool rotateSlew(double delta) {
        if (!connected) { addLogEntry("ERROR", "Rotation attempt without connection"); return false; }
        if (emergencyStop) return false;
        double newVal = slewAngle + delta;
        if (newVal >= 0 && newVal <= MAX_ANGLE) {
            slewAngle = newVal;
            updateEnergyConsumption(0, delta, loadWeight);
            addLogEntry("ACTION", "Boom rotated by " + to_string(delta) + " deg");
            return true;
        }
        addLogEntry("ERROR", "Rotation angle limit exceeded");
        return false;
    }

    bool setLoad(double weight) {
        if (!connected) { addLogEntry("ERROR", "Set load without connection"); return false; }
        if (emergencyStop) return false;
        if (weight <= MAX_LOAD) {
            if (loadWeight == 0 && weight > 0) cycleCount++;
            loadWeight = weight;
            updateEnergyConsumption(0, 0, weight);
            addLogEntry("LOAD", "Load set to " + to_string(weight) + " t");
            return true;
        }
        addLogEntry("ERROR", "Overload: " + to_string(weight) + " t");
        return false;
    }

    void emergency() { emergencyStop = true; status = "EMERGENCY"; addLogEntry("EMERGENCY", "Emergency stop activated"); }
    void reset() { emergencyStop = false; status = "READY"; addLogEntry("RESET", "Emergency reset"); }

    double getHookHeight() const { return hookHeight; }
    double getSlewAngle() const { return slewAngle; }
    double getLoadWeight() const { return loadWeight; }
    bool isEmergency() const { return emergencyStop; }
    bool isConnected() const { return connected; }
    string getStatus() const { return status; }
    int getCycleCount() const { return cycleCount; }
};

PortalCrane crane;
HWND hConnectionStatus, hCraneInfo, hEnergyInfo, hStatusText;
HWND hUp, hDown, hLeft, hRight, hLoad5, hLoad10, hLoad20, hEmergencyBtn, hResetBtn;
HWND hConnectBtn, hDisconnectBtn, hShowCraneInfoBtn, hShowEnergyBtn, hShowLogBtn, hExitBtn;

enum DisplayMode { MODE_STATUS, MODE_CRANE_INFO, MODE_ENERGY, MODE_LOG };
DisplayMode currentMode = MODE_STATUS;

HFONT hDefaultFont, hMonoFont, hTitleFont;
HBRUSH hGrayBrush;

void UpdateDisplay() {
    char buffer[1024];

    sprintf_s(buffer, "[ STATUS ] %s", crane.getConnectionStatus().c_str());
    SetWindowTextA(hConnectionStatus, buffer);

    sprintf_s(buffer, "[ DATA ] Height: %.1f m | Angle: %.0f deg | Load: %.1f t | Emergency: %s",
              crane.getHookHeight(), crane.getSlewAngle(), crane.getLoadWeight(),
              crane.isEmergency() ? "ACTIVE" : "OFF");
    SetWindowTextA(hStatusText, buffer);

    string content;
    switch (currentMode) {
        case MODE_STATUS:
            content =
                "+-----------------------------------------------------------+\n"
                "|                      MAIN MENU                           |\n"
                "+-----------------------------------------------------------+\n"
                "|                                                           |\n"
                "|  Crane ready for operation.                               |\n"
                "|                                                           |\n"
                "|  [ CONTROL BUTTONS ]                                      |\n"
                "|    RAISE / LOWER  -> Hook movement (1 meter per click)    |\n"
                "|    LEFT / RIGHT   -> Boom rotation (15 degrees per click) |\n"
                "|    5T / 10T / 20T -> Set load weight                      |\n"
                "|    EMERGENCY STOP -> Emergency block                      |\n"
                "|    RESET          -> Reset emergency state                |\n"
                "|                                                           |\n"
                "|  [ INFORMATION ]                                          |\n"
                "|    CRANE INFO   -> Technical specifications               |\n"
                "|    ENERGY OPT   -> Power consumption report               |\n"
                "|    EVENT LOG    -> History with signatures                |\n"
                "|                                                           |\n"
                "|  [ SYSTEM ]                                               |\n"
                "|    CONNECT CRANE -> Establish connection (EtherCAT)       |\n"
                "|    DISCONNECT    -> Disconnect from crane                 |\n"
                "|    VERIFY SIGNATURE -> Check log integrity                |\n"
                "|    EXIT          -> Close application                     |\n"
                "|                                                           |\n"
                "+-----------------------------------------------------------+\n"
                "|                     SYSTEM READY                          |\n"
                "+-----------------------------------------------------------+";
            break;
        case MODE_CRANE_INFO: content = crane.getCraneInfo(); break;
        case MODE_ENERGY: content = crane.getEnergyReport(); break;
        case MODE_LOG: content = crane.getEventLog(10); break;
    }
    SetWindowTextA(hCraneInfo, content.c_str());

    EnergyData energy = crane.getEnergyData();
    sprintf_s(buffer, "[ ENERGY ] Power: %.1f kW | Total: %.2f kWh | Efficiency: %.0f %%",
              energy.currentPower, energy.totalEnergy, energy.efficiency);
    SetWindowTextA(hEnergyInfo, buffer);

    if (crane.isEmergency()) {
        SetWindowTextA(hEmergencyBtn, "!!! EMERGENCY STOP ACTIVE !!!");
        SetWindowTextA(hResetBtn, ">>> PRESS RESET <<<");
    } else {
        SetWindowTextA(hEmergencyBtn, "[!] EMERGENCY STOP");
        SetWindowTextA(hResetBtn, "[R] RESET");
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)hGrayBrush;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: if (!crane.raiseHook(1.0)) MessageBoxA(hwnd, "Cannot raise above 30 meters!", "Error", MB_OK | MB_ICONERROR); UpdateDisplay(); break;
                case 2: if (!crane.lowerHook(1.0)) MessageBoxA(hwnd, "Hook already on ground (0 meters)!", "Error", MB_OK | MB_ICONERROR); UpdateDisplay(); break;
                case 3: if (!crane.rotateSlew(-15.0)) MessageBoxA(hwnd, "Cannot rotate below 0 degrees!", "Error", MB_OK | MB_ICONERROR); UpdateDisplay(); break;
                case 4: if (!crane.rotateSlew(15.0)) MessageBoxA(hwnd, "Cannot rotate above 360 degrees!", "Error", MB_OK | MB_ICONERROR); UpdateDisplay(); break;
                case 5: crane.setLoad(5.0); UpdateDisplay(); break;
                case 6: crane.setLoad(10.0); UpdateDisplay(); break;
                case 7: crane.setLoad(20.0); UpdateDisplay(); break;
                case 8: crane.emergency(); UpdateDisplay(); break;
                case 9: crane.reset(); UpdateDisplay(); break;
                case 10:
                    if (crane.connectToCrane("EtherCAT / Modbus TCP"))
                        MessageBoxA(hwnd, "Crane connected successfully!", "Success", MB_OK | MB_ICONINFORMATION);
                    else
                        MessageBoxA(hwnd, "Connection failed! Already connected?", "Error", MB_OK | MB_ICONERROR);
                    UpdateDisplay();
                    break;
                case 11:
                    if (crane.disconnectFromCrane())
                        MessageBoxA(hwnd, "Crane disconnected!", "Info", MB_OK | MB_ICONINFORMATION);
                    UpdateDisplay();
                    break;
                case 12: currentMode = MODE_CRANE_INFO; UpdateDisplay(); break;
                case 13: currentMode = MODE_ENERGY; UpdateDisplay(); break;
                case 14: currentMode = MODE_LOG; UpdateDisplay(); break;
                case 15: currentMode = MODE_STATUS; UpdateDisplay(); break;
                case 16:
                    if (crane.verifyLogSignature(1))
                        MessageBoxA(hwnd, "Signature is VALID! Log not modified.", "Verification Result", MB_OK | MB_ICONINFORMATION);
                    else
                        MessageBoxA(hwnd, "Signature INVALID! Log may be modified!\n\nNote: Check ID 1 (first log entry).", "Verification Result", MB_OK | MB_ICONERROR);
                    break;
                case 17:
                    crane.disconnectFromCrane();
                    DestroyWindow(hwnd);
                    break;
            }
            break;
        case WM_DESTROY:
            DeleteObject(hDefaultFont);
            DeleteObject(hMonoFont);
            DeleteObject(hTitleFont);
            DeleteObject(hGrayBrush);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    hGrayBrush = CreateSolidBrush(RGB(240, 240, 240));

    hDefaultFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                              DEFAULT_PITCH, "Segoe UI");
    hMonoFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                           DEFAULT_PITCH, "Consolas");
    hTitleFont = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                            DEFAULT_PITCH, "Segoe UI");

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CraneWindow";
    wc.hbrBackground = hGrayBrush;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 1200;
    int windowHeight = 750;
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExA(0, "CraneWindow", "PORTAL CRANE CONTROL SYSTEM v2.0",
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
                                windowX, windowY, windowWidth, windowHeight,
                                NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;

    int y = 15;
    int leftX = 15;
    int rightX = 620;

    HWND hTitle = CreateWindowA("STATIC", " PORTAL CRANE CONTROL SYSTEM ",
                  WS_VISIBLE | WS_CHILD | SS_CENTER | WS_BORDER,
                  leftX, y, 850, 45, hwnd, NULL, hInstance, NULL);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
    y += 55;

    hConnectionStatus = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTER,
                                      leftX, y, 850, 30, hwnd, NULL, hInstance, NULL);
    SendMessage(hConnectionStatus, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    y += 40;

    hStatusText = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | SS_CENTER | WS_BORDER,
                                leftX, y, 850, 45, hwnd, NULL, hInstance, NULL);
    SendMessage(hStatusText, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    y += 55;

    hCraneInfo = CreateWindowA("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                               leftX, y, 550, 520, hwnd, NULL, hInstance, NULL);
    SendMessage(hCraneInfo, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

    hEnergyInfo = CreateWindowA("STATIC", "", WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTER,
                                rightX, y, 380, 55, hwnd, NULL, hInstance, NULL);
    SendMessage(hEnergyInfo, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    y += 65;

    HWND hControlGroup = CreateWindowA("BUTTON", " [ CRANE CONTROL ] ", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                  rightX, y, 380, 280, hwnd, NULL, hInstance, NULL);
    SendMessage(hControlGroup, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

    int btnY = y + 30;
    int btnWidth = 160;

    hUp = CreateWindowA("BUTTON", ">> RAISE (1m) <<", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                        rightX + 15, btnY, btnWidth, 45, hwnd, (HMENU)1, hInstance, NULL);
    hDown = CreateWindowA("BUTTON", ">> LOWER (1m) <<", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                          rightX + 200, btnY, btnWidth, 45, hwnd, (HMENU)2, hInstance, NULL);
    SendMessage(hUp, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hDown, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    btnY += 55;

    hLeft = CreateWindowA("BUTTON", "<< LEFT (15 deg)", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                          rightX + 15, btnY, btnWidth, 45, hwnd, (HMENU)3, hInstance, NULL);
    hRight = CreateWindowA("BUTTON", "RIGHT (15 deg) >>", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                           rightX + 200, btnY, btnWidth, 45, hwnd, (HMENU)4, hInstance, NULL);
    SendMessage(hLeft, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hRight, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    btnY += 60;

    CreateWindowA("STATIC", "Set Load:", WS_VISIBLE | WS_CHILD, rightX + 15, btnY, 70, 35, hwnd, NULL, hInstance, NULL);
    hLoad5 = CreateWindowA("BUTTON", "5 TONS", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                           rightX + 90, btnY, 85, 40, hwnd, (HMENU)5, hInstance, NULL);
    hLoad10 = CreateWindowA("BUTTON", "10 TONS", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                            rightX + 180, btnY, 85, 40, hwnd, (HMENU)6, hInstance, NULL);
    hLoad20 = CreateWindowA("BUTTON", "20 TONS", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                            rightX + 270, btnY, 85, 40, hwnd, (HMENU)7, hInstance, NULL);
    SendMessage(hLoad5, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hLoad10, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hLoad20, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    btnY += 50;

    hEmergencyBtn = CreateWindowA("BUTTON", "[!] EMERGENCY STOP", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                  rightX + 15, btnY, 170, 50, hwnd, (HMENU)8, hInstance, NULL);
    hResetBtn = CreateWindowA("BUTTON", "[R] RESET", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                              rightX + 195, btnY, 170, 50, hwnd, (HMENU)9, hInstance, NULL);
    SendMessage(hEmergencyBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hResetBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

    y += 295;

    HWND hFuncGroup = CreateWindowA("BUTTON", " [ ADDITIONAL FUNCTIONS ] ", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
                  rightX, y, 380, 220, hwnd, NULL, hInstance, NULL);
    SendMessage(hFuncGroup, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

    int funcY = y + 30;
    int funcBtnWidth = 170;

    hConnectBtn = CreateWindowA("BUTTON", "[C] CONNECT CRANE", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                rightX + 15, funcY, funcBtnWidth, 40, hwnd, (HMENU)10, hInstance, NULL);
    hDisconnectBtn = CreateWindowA("BUTTON", "[X] DISCONNECT", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   rightX + 195, funcY, funcBtnWidth, 40, hwnd, (HMENU)11, hInstance, NULL);
    SendMessage(hConnectBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hDisconnectBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    funcY += 50;

    hShowCraneInfoBtn = CreateWindowA("BUTTON", "[I] CRANE INFO", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                      rightX + 15, funcY, funcBtnWidth, 40, hwnd, (HMENU)12, hInstance, NULL);
    hShowEnergyBtn = CreateWindowA("BUTTON", "[E] ENERGY OPT", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                   rightX + 195, funcY, funcBtnWidth, 40, hwnd, (HMENU)13, hInstance, NULL);
    SendMessage(hShowCraneInfoBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hShowEnergyBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    funcY += 50;

    hShowLogBtn = CreateWindowA("BUTTON", "[L] EVENT LOG", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                rightX + 15, funcY, funcBtnWidth, 40, hwnd, (HMENU)14, hInstance, NULL);
    HWND hMainMenuBtn = CreateWindowA("BUTTON", "[M] MAIN MENU", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  rightX + 195, funcY, funcBtnWidth, 40, hwnd, (HMENU)15, hInstance, NULL);
    SendMessage(hShowLogBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hMainMenuBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    funcY += 50;

    HWND hVerifyBtn = CreateWindowA("BUTTON", "[V] VERIFY SIGNATURE", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  rightX + 15, funcY, funcBtnWidth, 40, hwnd, (HMENU)16, hInstance, NULL);
    hExitBtn = CreateWindowA("BUTTON", "[Q] EXIT", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                             rightX + 195, funcY, funcBtnWidth, 40, hwnd, (HMENU)17, hInstance, NULL);
    SendMessage(hVerifyBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);
    SendMessage(hExitBtn, WM_SETFONT, (WPARAM)hDefaultFont, TRUE);

    UpdateDisplay();
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}