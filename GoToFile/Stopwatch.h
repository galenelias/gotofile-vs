#pragma once

class Stopwatch
{
private:
	LARGE_INTEGER StartingTime;
	LARGE_INTEGER Frequency;

public:
	Stopwatch()
	{
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&StartingTime);
	}

	LONGLONG GetElapsedMicroseconds() const
	{
		LARGE_INTEGER EndingTime, ElapsedMicroseconds;
		QueryPerformanceCounter(&EndingTime);
		ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;

		// We now have the elapsed number of ticks, along with the
		// number of ticks-per-second. We use these values
		// to convert to the number of elapsed microseconds.
		// To guard against loss-of-precision, we convert
		// to microseconds *before* dividing by ticks-per-second.
		ElapsedMicroseconds.QuadPart *= 1000000;
		ElapsedMicroseconds.QuadPart /= Frequency.QuadPart;

		return ElapsedMicroseconds.QuadPart;
	}
};


