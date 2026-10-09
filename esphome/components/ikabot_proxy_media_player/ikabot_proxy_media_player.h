#pragma once

#include "esphome/components/media_player/media_player.h"
#include "esphome/core/component.h"

#include <string>

namespace esphome::ikabot_proxy_media_player {

class IkabotProxyMediaPlayer final : public Component, public media_player::MediaPlayer {
 public:
  void setup() override;
  void dump_config() override;

  media_player::MediaPlayerTraits get_traits() override;

  // Home Assistant confirms the real external speaker state through ESPHome
  // API actions. These transitions are what the Voice Assistant sees.
  void external_started();
  void external_finished();
  void external_failed();
  // Cancel a silent response without waiting for an external TTS FINISH ACK.
  void cancel_pending();

  const std::string &get_pending_url() const { return this->pending_url_; }

 protected:
  void control(const media_player::MediaPlayerCall &call) override;
  void set_state_(media_player::MediaPlayerState state);

  std::string pending_url_{};
  bool awaiting_external_finish_{false};
};

}  // namespace esphome::ikabot_proxy_media_player
