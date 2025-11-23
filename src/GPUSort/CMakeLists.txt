
## Sources required to build FFXParallelSort and its supporting code
set(SORT_SOURCES
	# C++ sources
	${CMAKE_CURRENT_LIST_DIR}/DeviceRadixSort.cpp
	${CMAKE_CURRENT_LIST_DIR}/EmulatedDeadlocking.cpp
	${CMAKE_CURRENT_LIST_DIR}/FFXParallelSort.cpp
	${CMAKE_CURRENT_LIST_DIR}/ForwardSweep.cpp
	${CMAKE_CURRENT_LIST_DIR}/OneSweep.cpp
	${CMAKE_CURRENT_LIST_DIR}/pch.cpp

	# Headers (listed so IDEs can see them via the variable)
	# ${CMAKE_CURRENT_LIST_DIR}/ComputeKernelBase.h
	# ${CMAKE_CURRENT_LIST_DIR}/DeviceRadixSort.h
	# ${CMAKE_CURRENT_LIST_DIR}/DeviceRadixSortKernels.h
	# ${CMAKE_CURRENT_LIST_DIR}/EmulatedDeadlocking.h
	# ${CMAKE_CURRENT_LIST_DIR}/EmulatedDeadlockingKernels.h
	# ${CMAKE_CURRENT_LIST_DIR}/FFXParallelSort.h
	# ${CMAKE_CURRENT_LIST_DIR}/FFXParallelSortKernels.h
	# ${CMAKE_CURRENT_LIST_DIR}/ForwardSweep.h
	# ${CMAKE_CURRENT_LIST_DIR}/GPUSortBase.h
	# ${CMAKE_CURRENT_LIST_DIR}/GPUSorting.h
	# ${CMAKE_CURRENT_LIST_DIR}/pch.h
	# ${CMAKE_CURRENT_LIST_DIR}/SweepBase.h
	# ${CMAKE_CURRENT_LIST_DIR}/SweepCommonKernels.h
	# ${CMAKE_CURRENT_LIST_DIR}/Tests.h
	# ${CMAKE_CURRENT_LIST_DIR}/Tuner.h
	# ${CMAKE_CURRENT_LIST_DIR}/UtilityKernels.h
	# ${CMAKE_CURRENT_LIST_DIR}/Utils.h
)

# Export the variable to the parent scope so the caller can consume the list
set(SORT_SOURCES "${SORT_SOURCES}" PARENT_SCOPE)