/* ============================================================
University of Illinois at Urbana Champaign
Breadth-First-Search Optimized
Author: Luis Remis
Date:	Apr 5 2015
============================================================ */
#include "Chrono.h"

#include <cmath>
#include <iostream>
#include <sstream>

using namespace std;

// *****************************************************************************
// Public methods definitions
// *****************************************************************************
Chrono::Chrono(const string& name, const bool asyncEnabled) :
	 name         (name)
	,enabled      (true)
{
	elapsedStats.name = "elapsedStats";
	periodStats.name = "periodStats";
	reset();
}

Chrono::~Chrono(void)
{
}

void Chrono::tic(void)
{
	if (!enabled){
		return;
	}

	if (ticIdle){
		ticIdle = false;
		doTic();
	} else{
		++errors;
		cerr << "Chrono::tic - " << name << ": Calling Chrono::tic with no matching Chrono::tag!" << endl;
	}
}

void Chrono::tac(void)
{
	if (!enabled){
		return;
	}

	if (!ticIdle){
		ticIdle = true;
		doTac();
	} else{
		++errors;
		cerr << "Chrono::tac - " << name << ": Calling Chrono::tac with no matching Chrono::tic!" << endl;
	}
}

void Chrono::reset(void)
{
	ticIdle = true;
	errors = 0;
	resetStats(elapsedStats);
	resetStats(periodStats);
}

void Chrono::setEnabled(const bool val)
{
	enabled = val;
}

const Chrono::ChronoStats& Chrono::getElapsedStats(void) const
{
	return elapsedStats;
}

const Chrono::ChronoStats& Chrono::getPeriodStats(void) const
{
	return periodStats;
}

std::ostream& Chrono::printStats(const Chrono::ChronoStats& stats, std::ostream& os) const
{
	os.precision(2);
	os << fixed;
	os << name << ": " << stats.name << endl;
	os << "\terrors: " 		<< errors << endl;
	os << "\ttotalTime: "   << stats.totalTime_ms   << " [ms]" << endl;
	os << "\taverageTime: " << stats.averageTime_ms << " [ms]" << endl;
	os << "\tstdDevTime: "  << stats.stdDevTime_ms  << " [ms]" << endl;
	os << "\tlastTime: "    << stats.lastTime_ms    << " [ms]" << endl;
	os << "\tminTime: "     << stats.minTime_ms     << " [ms]" << endl;
	os << "\tmaxTime: "     << stats.maxTime_ms     << " [ms]" << endl;

	return os;
}

std::ostream& Chrono::printAvgTime(const Chrono::ChronoStats& stats, std::ostream& os) const
{
	os.precision(2);
	os << fixed;
	os << name << ": " << stats.name << " -> " << "averageTime: " << stats.averageTime_ms << " [ms]" << endl;

	return os;
}

std::ostream& Chrono::printAvgTime(const Chrono::ChronoStats& stats, std::ostream& os, const float ref) const
{
	os.precision(2);
	os << fixed;
	os << name << ": " << stats.name << " -> " << "averageTime: " << stats.averageTime_ms << " [ms] (";
	os << (stats.averageTime_ms/ref*100.0f) << "%)" << endl;

	return os;
}


// *****************************************************************************
// Private/Protected methods definitions
// *****************************************************************************
void Chrono::resetStats(ChronoStats& stats)
{
	stats.counter = 0;
	stats.totalTime_ms = 0.0f;
	stats.totalSquaredTime_ms2 = 0.0f;
	stats.averageTime_ms = 0.0f;
	stats.stdDevTime_ms = 0.0f;
	stats.lastTime_ms = 0.0f;
	stats.minTime_ms = 0.0f;
	stats.maxTime_ms = 0.0f;
}

void Chrono::updateStats(ChronoStats& stats)
{
	++stats.counter;
	stats.totalTime_ms += stats.lastTime_ms;
	stats.totalSquaredTime_ms2 += stats.lastTime_ms * stats.lastTime_ms;
	stats.averageTime_ms = stats.totalTime_ms / (float)stats.counter;
	stats.stdDevTime_ms = sqrtf(stats.totalSquaredTime_ms2 / (float)stats.counter - stats.averageTime_ms * stats.averageTime_ms);
	if (stats.counter > 1){
		stats.maxTime_ms = max(stats.lastTime_ms, stats.maxTime_ms);
		stats.minTime_ms = min(stats.lastTime_ms, stats.minTime_ms);
	} else{
		stats.maxTime_ms = stats.lastTime_ms;
		stats.minTime_ms = stats.lastTime_ms;
	}
}

