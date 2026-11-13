#pragma once
#include "Session.h"
capture_engine *capture_engine_create_with_factory(const capture_options *,capture_callback,void *,capture_result *,capture::SessionFactory);
