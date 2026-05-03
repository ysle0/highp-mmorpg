package main

import (
	"log/slog"
	"os"

	"github.com/ysle0/highp-mmorpg/exec/chat/chat-load-client-v2/cfg"
)

var (
	logger = slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
		AddSource: true,
		Level:     slog.LevelDebug,
	}))
)

func main() {
	if len(os.Args) < 2 {
		logger.Error("Usage: chat-load-client-v2 <scenario_dir_name>")
		logger.Error("  e.g. chat-load-client-v2 baseline-idle")
		os.Exit(1)
	}

	scenarioName := os.Args[1]
	if scenarioName == "" {
		logger.Error("")
	}

	tomlPath := "scenarios/" + scenarioName + "/scenario.toml"
	config, err := cfg.NewWithFilepath(tomlPath)
	if err != nil {
		logger.Error("Failed to decode TOML file", "path", tomlPath, "error", err)
		os.Exit(1)
	}

	logger.Info("Load test scenario loaded",
		"name", config.Name,
		"clients", config.ClientCount,
		"phases", len(config.Phases),
	)
}
