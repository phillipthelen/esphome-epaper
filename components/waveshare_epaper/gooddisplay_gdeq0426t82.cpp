#include "gooddisplay_gdeq0426t82.h"

#include <cstdint>

#include "esphome/core/log.h"

namespace esphome {
namespace waveshare_epaper {

const char *const GDEQ0426T82::TAG = "gdeq0426t82";

int GDEQ0426T82::get_width_internal() { return WIDTH; }

int GDEQ0426T82::get_height_internal() { return HEIGHT; }

uint32_t GDEQ0426T82::idle_timeout_() { return IDLE_TIMEOUT; }

void GDEQ0426T82::set_full_update_every(uint32_t full_update_every) {
  this->full_update_every_ = full_update_every;
}

void GDEQ0426T82::full_refresh() {
  this->at_update_ = 0;
  this->update();
}

void GDEQ0426T82::initialize() {}

void GDEQ0426T82::display() {
  RefreshMode mode = this->at_update_ == 0 ? FULL_REFRESH : PARTIAL_REFRESH;
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  this->init_display_();

  this->write_buffer_(mode);

  this->status_clear_warning();

  this->deep_sleep();
}

void GDEQ0426T82::init_display_() {
  reset_();
  delay(10);
  this->command(0x12);  //SWRESET
  delay(10); // 10ms according to specs
  this->command(0x18);
  this->data(0x80);
  this->command(0x0C);
  this->data(0xAE);
  this->data(0xC7);
  this->data(0xC3);
  this->data(0xC0);
  this->data(0x80);
  this->command(0x01); // Driver output control
  this->data((HEIGHT - 1) % 256); // gates A0..A7
  this->data((HEIGHT - 1) / 256); // gates A8, A9
  this->data(0x02); // SM (interlaced) ??
  this->command(0x3C); // BorderWavefrom
  this->data(0x01);
}

void GDEQ0426T82::write_buffer_(RefreshMode mode) {
  this->command(0x21); // Display Update Controll
  this->data(0x40);    // bypass RED as 0
  this->data(0x00); 
  switch (mode) {
    case FULL_REFRESH:
      this->clear_();
      for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
        oldData_[i] = 0xff;
      }
      this->write_buffer_(PARTIAL_REFRESH);
      return;

    case FAST_REFRESH:
    this->command(0x1A); // Write to temperature register
    this->data(0x5A);
    this->command(0x22);
    this->data(0xd7);

      // Write old Data
      this->command(0x26);
      this->start_data_();
      for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
        this->write_byte(0x00);
      this->end_data_();

      // Write new Data
      this->command(0x24);  // writes New data to SRAM.
      this->start_data_();
      for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
        this->write_byte(~this->buffer_[i]);
        oldData_[i] = this->buffer_[i];
      }
      this->end_data_();
      break;

    case PARTIAL_REFRESH:
      this->command(0x21); // Display Update Controll
      this->data(0x00);    // RED normal
      this->data(0x00);    // single chip application
      this->command(0x22);
      this->data(0xfc);
      this->command(0x20);

      this->command(0x11); // set ram entry mode
      this->data(0x01);    // x increase, y decrease : y reversed
      this->command(0x44);
      this->data(x % 256);
      this->data(x / 256);
      this->data((x + w - 1) % 256);
      this->data((x + w - 1) / 256);
      this->command(0x45);
      this->data((y + h - 1) % 256);
      this->data((y + h - 1) / 256);
      this->data(y % 256);
      this->data(y / 256);
      this->command(0x4e);
      this->data(x % 256);
      this->data(x / 256);
      this->command(0x4f);
      this->data((y + h - 1) % 256);
      this->data((y + h - 1) / 256);

      this->command(0x26);
      this->start_data_();
      this->write_array(this->oldData_, this->get_buffer_length_());
      this->end_data_();

      this->command(0x24);  // writes New data to SRAM.
      this->start_data_();
      this->write_array(this->buffer_, this->get_buffer_length_());
      this->end_data_();
      for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
        oldData_[i] = this->buffer_[i];

      this->command(0x92);
      break;

    default:
      ESP_LOGE(TAG, "unsupported refresh mode, mode:%d", mode);
      return;
  }

  this->command(0x12);  // DISPLAY update
  delay(1);             //!!!The delay here is necessary, 200uS at least!!!
  if (!this->wait_until_idle_())  // waiting for the electronic paper IC to
                                  // release the idle signal
  {
    this->status_set_warning();
    return;
  }
}

void GDEQ0426T82::clear_() {
  // Write old Data
  this->command(0x26);
  this->start_data_();
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();

  // Write new Data
  this->command(0x24);
  this->start_data_();
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++)
    this->write_byte(0x00);
  this->end_data_();

  this->command(0x12);  // DISPLAY update
  delay(1);             //!!!The delay here is necessary, 200uS at least!!!
  if (!this->wait_until_idle_())  // waiting for the electronic paper IC to
                                  // release the idle signal
  {
    this->status_set_warning();
    return;
  }
}

void GDEQ0426T82::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(10);
  }
}

void GDEQ0426T82::deep_sleep() {
  this->command(0x50);  // VCOM AND DATA INTERVAL SETTING
  this->data(0xf7);     // WBmode:VBDF 17|D7 VBDW 97 VBDB 57    WBRmode:VBDF F7
                        // VBDW 77 VBDB 37  VBDR B7

this->command(0x22);
    this->data(0x83);
    this->command(0x20);
  if (!this->wait_until_idle_())  // waiting for the electronic paper IC to
                                  // release the idle signal
    return;

  this->command(0x10);  // deep sleep
  this->data(0x1);
}

void GDEQ0426T82::dump_config() {
  LOG_DISPLAY("", "Good Display e-Paper", this)
  ESP_LOGCONFIG(TAG, "  Model: GDEQ0426T82");
  LOG_PIN("  CS Pin: ", this->cs_)
  LOG_PIN("  Reset Pin: ", this->reset_pin_)
  LOG_PIN("  DC Pin: ", this->dc_pin_)
  LOG_PIN("  Busy Pin: ", this->busy_pin_)
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace waveshare_epaper
}  // namespace esphome