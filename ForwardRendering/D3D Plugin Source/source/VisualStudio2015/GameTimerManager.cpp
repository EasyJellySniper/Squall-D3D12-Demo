#include "GameTimerManager.h"
#include <iostream>
#include <iomanip>
using namespace std;
#include "GraphicManager.h"

inline void cls(HANDLE hConsole)
{
    COORD coordScreen = { 0, 0 };    // home for the cursor 
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    // Get the number of character cells in the current buffer. 

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return;
    }

    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    // Fill the entire screen with blanks.

    if (!FillConsoleOutputCharacter(hConsole,        // Handle to console screen buffer 
        (TCHAR)' ',     // Character to write to the buffer
        dwConSize,       // Number of cells to write 
        coordScreen,     // Coordinates of first cell 
        &cCharsWritten))// Receive number of characters written
    {
        return;
    }

    // Get the current text attribute.

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return;
    }

    // Set the buffer's attributes accordingly.

    if (!FillConsoleOutputAttribute(hConsole,         // Handle to console screen buffer 
        csbi.wAttributes, // Character attributes to use
        dwConSize,        // Number of cells to set attribute 
        coordScreen,      // Coordinates of first cell 
        &cCharsWritten)) // Receive number of characters written
    {
        return;
    }

    // Put the cursor at its home coordinates.

    SetConsoleCursorPosition(hConsole, coordScreen);
}

void GameTimerManager::PrintGameTime()
{
    chrono::steady_clock::time_point currTime = std::chrono::steady_clock::now();
    profileTime += std::chrono::duration_cast<std::chrono::milliseconds>(currTime - lastTime).count();

	if (profileTime > 1000.0)
	{
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        cls(hStdout);

		cout << "-------------- CPU Profile(ms) --------------\n";
		cout << fixed << setprecision(4);
		cout << "Wait GPU Fence: " << gameTime.updateTime << endl;
		cout << "Culling Time: " << gameTime.cullingTime << endl;
		cout << "Sorting Time: " << gameTime.sortingTime << endl;
		cout << "Render Time: " << gameTime.renderTime << endl;

		int totalDrawCall = 0;
		for (int i = 0; i < GraphicManager::Instance().GetThreadCount() - 1; i++)
		{
			cout << "\tThread " << i << " Time:" << gameTime.renderThreadTime[i] << endl;
			totalDrawCall += gameTime.batchCount[i];
		}

		cout << "Total DrawCall: " << totalDrawCall << endl;

		cout << "-------------- GPU Profile(ms) --------------\n";
		cout << "GPU Time: " << gameTime.gpuTime << endl;

        profileTime = 0.0;
	}

    lastTime = std::chrono::steady_clock::now();
}
