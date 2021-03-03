#pragma once

enum class dte_mode;

class DeviceIf {
public:
    virtual bool setup_data_mode() = 0;
    virtual bool set_mode(dte_mode mode) = 0;
};