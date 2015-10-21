/* ============================================================
University of Illinois at Urbana Champaign
Breadth-First-Search Optimized
Author: Luis Remis
Date:	Apr 5 2015
============================================================ */

#ifndef CHRONO_CPU_H_
#define CHRONO_CPU_H_

#include "Chrono.h"

#include <time.h>

// *****************************************************************************
// Class definitions
// *****************************************************************************
class ChronoCpu : public Chrono {
public:
	ChronoCpu(const std::string& name);
	~ChronoCpu(void);

protected:
	timespec lastTicTime;
	timespec ticTime;
	timespec tacTime;

	uint32_t ticCounter;

	virtual void doTic(void);
	virtual void doTac(void);
};


#endif /* CHRONO_CPU_H_ */
