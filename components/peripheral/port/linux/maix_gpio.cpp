#include "maix_gpio.hpp"
#include "maix_basic.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/gpio.h>
#include <algorithm>
#include <sys/ioctl.h>

namespace maix::peripheral::gpio
{
	GPIO::GPIO(std::string pin, gpio::Mode mode, gpio::Pull pull)
	{
		this->_pin = pin;
		this->_mode = mode;
		this->_pull = pull;
		this->_fd = 0;
		this->_line = 0;

		// to upper case first
		// convert B14/GPIOB14 to chip_id and offset, B can be any letter
		// if pin is number, throw not implemented error
		int chip_id = 0;
		int offset = 0;
		std::transform(pin.begin(), pin.end(), pin.begin(), ::toupper);
		if (pin.find("GPIO") != std::string::npos)
		{
			pin = pin.substr(4);
		}
		if (pin[0] >= 'A' && pin[0] <= 'Z')
		{
			chip_id = pin[0] - 'A';
			try
			{
				offset = std::stoi(pin.substr(1));
			}
			catch (const std::exception &e)
			{
				throw err::Exception(err::Err::ERR_NOT_IMPL, "pin format error");
			}
		}
		else
		{
			throw err::Exception(err::Err::ERR_NOT_IMPL, "GPIO pin only number not implemented in this platform");
		}
#if PLATFORM_MAIXCAM
		if (chip_id == 'P' - 'A') // GPIOP is /dev/gpiochip4
			chip_id = 4;
#endif
		// open gpiochip
		std::string chip_path = "/dev/gpiochip" + std::to_string(chip_id);
		int fd = open(chip_path.c_str(), O_RDWR);
		if (fd < 0)
		{
			throw err::Exception(err::Err::ERR_IO, "open " + chip_path + " failed");
		}
		// get gpio line
		struct gpiohandle_request req;
		memset(&req, 0, sizeof(req));
		req.lineoffsets[0] = offset;
		req.lines = 1;
		if (mode == gpio::Mode::IN)
			req.flags = GPIOHANDLE_REQUEST_INPUT;
		else if (mode == gpio::Mode::OUT)
			req.flags = GPIOHANDLE_REQUEST_OUTPUT;
		else if (mode == gpio::Mode::OUT_OD)
			req.flags = GPIOHANDLE_REQUEST_OUTPUT | GPIOHANDLE_REQUEST_OPEN_DRAIN;
		req.default_values[0] = pull == gpio::Pull::PULL_UP ? 1 : 0;
		strncpy(req.consumer_label, "maix_gpio", sizeof(req.consumer_label));
		if (ioctl(fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0)
		{
			close(fd);
			throw err::Exception(err::Err::ERR_IO, "get gpio line failed");
		}
		// save gpio line
		this->_fd = fd;
		this->_line = req.fd;
	}

	GPIO::~GPIO()
	{
		if (this->_line > 0)
			close(this->_line);
		if (this->_fd > 0)
			close(this->_fd);
	}

	int GPIO::value(int value)
	{
		struct gpiohandle_data data;
		memset(&data, 0, sizeof(data));
		if (value >= 0)
		{
			data.values[0] = value;
			if (ioctl(this->_line, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
			{
				return (int)(-err::Err::ERR_IO);
			}
		}
		memset(&data, 0, sizeof(data));
		if (ioctl(this->_line, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
		{
			return (int)(-err::Err::ERR_IO);
		}
		return data.values[0];
	}

	void GPIO::high()
	{
		value(1);
	}

	void GPIO::low()
	{
		value(0);
	}

	void GPIO::toggle()
	{
		if (value() == 0)
			high();
		else
			low();
	}
}; // namespace maix::peripheral::gpio
