#include "ikabot_proxy_media_player.h"

#include "esphome/core/log.h"

namespace esphome::ikabot_proxy_media_player {

static const char *const TAG = "ikabot.proxy_media_player";

void IkabotProxyMediaPlayer::setup() {
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
}

void IkabotProxyMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Ikabot external-audio proxy media player");
}

media_player::MediaPlayerTraits IkabotProxyMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();

  // The proxy accepts an announcement URL and STOP. It deliberately does not
  // decode audio locally.
  traits.clear_feature_flags(
      media_player::MediaPlayerEntityFeature::BROWSE_MEDIA |
      media_player::MediaPlayerEntityFeature::VOLUME_SET |
      media_player::MediaPlayerEntityFeature::VOLUME_MUTE);
  traits.add_feature_flags(
      media_player::MediaPlayerEntityFeature::PLAY_MEDIA |
      media_player::MediaPlayerEntityFeature::MEDIA_ANNOUNCE |
      media_player::MediaPlayerEntityFeature::STOP);
  return traits;
}

void IkabotProxyMediaPlayer::set_state_(media_player::MediaPlayerState new_state) {
  if (this->state == new_state)
    return;
  this->state = new_state;
  this->publish_state();
  ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
}

void IkabotProxyMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  const auto &url = call.get_media_url();
  if (url.has_value()) {
    this->pending_url_ = url.value();
    this->awaiting_external_finish_ = true;

    // Important: announce immediately. ESPHome VoiceAssistant starts a short
    // playback timeout after handing us the URL. Reporting ANNOUNCING here
    // keeps STREAMING_RESPONSE alive while HA forwards the URL to the
    // ReSpeaker Lite. The real end is still authoritative: only
    // external_finished()/external_failed() can end the proxy playback.
    this->set_state_(media_player::MEDIA_PLAYER_STATE_ANNOUNCING);

    this->cancel_timeout("external-playback-watchdog");
    this->set_timeout("external-playback-watchdog", 80000, [this]() {
      if (!this->awaiting_external_finish_)
        return;
      ESP_LOGW(TAG, "External playback watchdog expired");
      this->awaiting_external_finish_ = false;
      this->pending_url_.clear();
      this->set_state_(media_player::MEDIA_PLAYER_STATE_IDLE);
    });
  }

  const auto &command = call.get_command();
  if (command.has_value() &&
      command.value() == media_player::MEDIA_PLAYER_COMMAND_STOP) {
    this->cancel_timeout("external-playback-watchdog");
    this->awaiting_external_finish_ = false;
    this->pending_url_.clear();
    this->set_state_(media_player::MEDIA_PLAYER_STATE_IDLE);
  }
}

void IkabotProxyMediaPlayer::external_started() {
  if (!this->awaiting_external_finish_) {
    ESP_LOGW(TAG, "External START received without pending playback");
    return;
  }
  // We already report ANNOUNCING from URL handoff to avoid the ESPHome
  // start-playback timeout. This ACK confirms that the Lite really started.
  ESP_LOGD(TAG, "External playback confirmed started");
}

void IkabotProxyMediaPlayer::external_finished() {
  if (!this->awaiting_external_finish_) {
    ESP_LOGW(TAG, "External FINISH received without pending playback");
    return;
  }
  this->cancel_timeout("external-playback-watchdog");
  this->awaiting_external_finish_ = false;
  this->pending_url_.clear();
  this->set_state_(media_player::MEDIA_PLAYER_STATE_IDLE);
  ESP_LOGD(TAG, "External playback confirmed finished");
}

void IkabotProxyMediaPlayer::cancel_pending() {
  // A silent OK_ACTION must not leave the VA waiting for an audio FINISH ACK.
  this->cancel_timeout("external-playback-watchdog");
  this->awaiting_external_finish_ = false;
  this->pending_url_.clear();
  this->set_state_(media_player::MEDIA_PLAYER_STATE_IDLE);
  ESP_LOGI(TAG, "Silent response: external playback cancelled");
}

void IkabotProxyMediaPlayer::external_failed() {
  this->cancel_timeout("external-playback-watchdog");
  this->awaiting_external_finish_ = false;
  this->pending_url_.clear();
  this->set_state_(media_player::MEDIA_PLAYER_STATE_IDLE);
  ESP_LOGW(TAG, "External playback failed");
}

}  // namespace esphome::ikabot_proxy_media_player
