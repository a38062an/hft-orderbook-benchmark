enable_testing()

include(FetchContent)

FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ---------------------------
# Unit tests
# ---------------------------

add_executable(hft_unit_tests
	unit/orderbook_contract_test.cpp
	unit/order_book_factory_test.cpp
	unit/array_order_book_test.cpp
	unit/hybrid_order_book_test.cpp
	unit/pool_order_book_test.cpp
	unit/orderbook_branch_stress_test.cpp
	unit/vector_order_book_branch_test.cpp
	unit/map_order_book_branch_test.cpp
	unit/lock_free_queue_test.cpp
	unit/fix_parser_test.cpp
	unit/matching_engine_test.cpp
	unit/metrics_collector_test.cpp
)

target_link_libraries(hft_unit_tests
	PRIVATE
		hft_core
		GTest::gtest_main
		GTest::gmock
)

target_include_directories(hft_unit_tests
	PRIVATE
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/libs
)

gtest_discover_tests(hft_unit_tests)

# ---------------------------
# Integration tests (optional)
# ---------------------------
if(HFT_ENABLE_INTEGRATION_TESTS)
	add_executable(hft_integration_tests
		integration/tcp_gateway_integration_test.cpp
	)

	target_link_libraries(hft_integration_tests
		PRIVATE
			hft_core
			GTest::gtest_main
	)

	target_include_directories(hft_integration_tests
		PRIVATE
			${CMAKE_SOURCE_DIR}/src
			${CMAKE_SOURCE_DIR}/libs
	)

	gtest_discover_tests(hft_integration_tests)
endif()

# ---------------------------
# Coverage (portable)
# ---------------------------
if(HFT_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
	# First try finding tools on PATH (Linux + Homebrew + custom LLVM installs)
	find_program(LLVM_PROFDATA llvm-profdata)
	find_program(LLVM_COV llvm-cov)

	# On Apple toolchains these tools often live in Xcode tool directories,
	# so we ask xcrun where they are as a fallback.
	if((NOT LLVM_PROFDATA OR NOT LLVM_COV) AND APPLE)
		find_program(XCRUN_EXECUTABLE xcrun)
		if(XCRUN_EXECUTABLE)
			execute_process(
				COMMAND ${XCRUN_EXECUTABLE} --find llvm-profdata
				OUTPUT_VARIABLE XCRUN_LLVM_PROFDATA
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)
			execute_process(
				COMMAND ${XCRUN_EXECUTABLE} --find llvm-cov
				OUTPUT_VARIABLE XCRUN_LLVM_COV
				OUTPUT_STRIP_TRAILING_WHITESPACE
				ERROR_QUIET
			)

			if(NOT LLVM_PROFDATA AND EXISTS "${XCRUN_LLVM_PROFDATA}")
				set(LLVM_PROFDATA "${XCRUN_LLVM_PROFDATA}")
			endif()
			if(NOT LLVM_COV AND EXISTS "${XCRUN_LLVM_COV}")
				set(LLVM_COV "${XCRUN_LLVM_COV}")
			endif()
		endif()
	endif()

	if(LLVM_PROFDATA AND LLVM_COV)
		set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)
		file(GLOB_RECURSE PRODUCTION_SOURCES CONFIGURE_DEPENDS
			"${CMAKE_SOURCE_DIR}/src/*.cpp"
			"${CMAKE_SOURCE_DIR}/src/*.hpp"
			"${CMAKE_SOURCE_DIR}/libs/*.hpp"
		)

		# `coverage` target workflow:
		# 1) run unit tests with profile instrumentation output
		# 2) merge .profraw -> .profdata
		# 3) print terminal report and generate HTML report
		add_custom_target(coverage
			COMMAND ${CMAKE_COMMAND} -E rm -rf ${COVERAGE_DIR}
			COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
			COMMAND ${CMAKE_COMMAND} -E env LLVM_PROFILE_FILE=${COVERAGE_DIR}/hft_unit_tests.profraw $<TARGET_FILE:hft_unit_tests>
			COMMAND ${LLVM_PROFDATA} merge -sparse ${COVERAGE_DIR}/hft_unit_tests.profraw -o ${COVERAGE_DIR}/hft_unit_tests.profdata
			COMMAND ${LLVM_COV} report $<TARGET_FILE:hft_unit_tests>
					-instr-profile=${COVERAGE_DIR}/hft_unit_tests.profdata
					${PRODUCTION_SOURCES}
			COMMAND ${LLVM_COV} show $<TARGET_FILE:hft_unit_tests>
					-instr-profile=${COVERAGE_DIR}/hft_unit_tests.profdata
					-format=html
					-output-dir=${COVERAGE_DIR}/html
					${PRODUCTION_SOURCES}
			DEPENDS hft_unit_tests
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMAND_EXPAND_LISTS
			COMMENT "Running unit tests and generating LLVM coverage report in build/coverage/html"
		)
	else()
		message(WARNING "Coverage enabled but llvm-profdata/llvm-cov were not found; coverage target disabled.")
	endif()
endif()
