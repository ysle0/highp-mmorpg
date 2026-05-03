package cfg

type PhaseType = string

var (
	PhaseTypeConnect       = "connect"
	PhaseTypeJoin          = "join"
	PhaseTypeHold          = "hold"
	PhaseTypeSteadySend    = "steady_send"
	PhaseTypeDisconnect    = "disconnect"
	PhaseTypeBurstSend     = "burst_send"
	PhaseTypeReconnectWave = "reconnect_wave"
	PhaseTypeMalformedSend = "malformed_send"
	PhaseTypeRoleSend      = "role_send"
)

type PhaseConfig struct {
	Type string `toml:"type"`
}

type ConnectPhaseConfig struct {
	PhaseConfig `toml:",inline"`
	TargetCount int `toml:"target_count"`
	RampDelayMs int `toml:"ramp_delay_ms"`
}

type HoldPhaseConfig struct {
	PhaseConfig `toml:",inline"`
	DurationSec int `toml:"duration_sec"`
}

type SteadySendPhaseConfig struct {
	PhaseConfig      `toml:",inline"`
	DurationSec      int `toml:"duration_sec"`
	SendIntervalMs   int `toml:"send_interval_ms"`
	MessageSizeBytes int `toml:"message_size_bytes"`
}

type BurstSendPhaseConfig struct {
	PhaseConfig      `toml:",inline"`
	BurstCount       int `toml:"burst_count"`
	BurstMessages    int `toml:"burst_messages"`
	IdleWindowMs     int `toml:"idle_window_ms"`
	MessageSizeBytes int `toml:"message_size_bytes"`
}

type ReconnectWavePhaseConfig struct {
	PhaseConfig        `toml:",inline"`
	WaveCount          int `toml:"wave_count"`
	HoldBetweenWaveSec int `toml:"hold_between_wave_sec"`
}

type MalformedSendPhaseConfig struct {
	PhaseConfig      `toml:",inline"`
	DurationSec      int `toml:"duration_sec"`
	SendIntervalMs   int `toml:"send_interval_ms"`
	MessageSizeBytes int `toml:"message_size_bytes"`
	MalformedPercent int `toml:"malformed_percent"`
}

type RoleSendPhaseConfig struct {
	PhaseConfig      `toml:",inline"`
	DurationSec      int `toml:"duration_sec"`
	SendIntervalMs   int `toml:"send_interval_ms"`
	MessageSizeBytes int `toml:"message_size_bytes"`
	SenderCount      int `toml:"sender_count"`
}
