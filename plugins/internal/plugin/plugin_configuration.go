//go:build windows

package plugin

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"strings"

	"github.com/spf13/viper"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/discovery"
	"github.com/tensorworks/directx-device-plugins/plugins/internal/mount"
	"go.uber.org/zap"
	"golang.org/x/exp/maps"
)

// PluginConfig represents the available configuration options for a device plugin
type PluginConfig struct {

	// The number of containers that can access each device simultaneously (set this to 1 for exclusive access)
	Multitenancy uint32

	// Specifies whether we advertise integrated devices (i.e. integrated GPUs)
	IncludeIntegrated bool

	// Specifies whether we advertise detachable devices (e.g. external GPUs)
	IncludeDetachable bool

	// The list of additional runtime files to be mounted to System32 for each device vendor
	AdditionalMounts map[string][]*discovery.RuntimeFile

	// The list of additional runtime files to be mounted to SysWOW64 for each device vendor
	AdditionalMountsWow64 map[string][]*discovery.RuntimeFile
}

// Appends a default set of mounts to the supplied mounts, converting all vendor names to lower case to ensure consistency
func appendMounts(mounts map[string][]*discovery.RuntimeFile, defaults map[string][]*discovery.RuntimeFile) map[string][]*discovery.RuntimeFile {

	// Gather the set of unique vendor names, converting all names to lower case
	vendors := make(map[string]bool)
	for _, vendor := range append(maps.Keys(mounts), maps.Keys(defaults)...) {
		vendorLower := strings.ToLower(vendor)
		if !vendors[vendorLower] {
			vendors[vendorLower] = true
		}
	}

	// Process the mounts for each vendor in turn
	appended := make(map[string][]*discovery.RuntimeFile)
	for vendor := range vendors {
		appended[vendor] = []*discovery.RuntimeFile{}

		// Add the mounts for the vendor if we have any
		vendorMounts, haveMounts := mounts[vendor]
		if haveMounts {
			appended[vendor] = append(appended[vendor], vendorMounts...)
		}

		// Add the defaults for the vendor if we have any
		vendorDefaults, haveDefaults := defaults[vendor]
		if haveDefaults {
			appended[vendor] = append(appended[vendor], vendorDefaults...)
		}
	}

	return appended
}

// Load loads the configuration data from the runtime environment.
func LoadConfig(pluginName string, logger *zap.SugaredLogger) (*PluginConfig, error) {

	// Set our default configuration values
	v := viper.New()
	v.SetDefault("multitenancy", 0)
	v.SetDefault("includeIntegrated", false)
	v.SetDefault("includeDetachable", false)
	v.SetDefault("additionalMounts", make(map[string][]*discovery.RuntimeFile))
	v.SetDefault("additionalMountsWow64", make(map[string][]*discovery.RuntimeFile))

	// The names of our environment variables reflect the plugin name
	envPrefix := fmt.Sprint(strings.ToUpper(pluginName), "_DEVICE_PLUGIN_")
	v.BindEnv("multitenancy", fmt.Sprint(envPrefix, "MULTITENANCY"))
	v.BindEnv("includeIntegrated", fmt.Sprint(envPrefix, "INCLUDE_INTEGRATED"))
	v.BindEnv("includeDetachable", fmt.Sprint(envPrefix, "INCLUDE_DETACHABLE"))

	// Check if a config file path was explicitly specified through an environment variable
	configPath, configPathExists := os.LookupEnv(fmt.Sprint(envPrefix, "CONFIG_FILE"))
	if configPathExists {

		// Verify that the specified value is an absolute path
		if !filepath.IsAbs(configPath) {
			return nil, errors.New("configuration file path must be an absolute path")
		}

		// Verify that the specified file exists
		if _, err := os.Stat(configPath); errors.Is(err, fs.ErrNotExist) {
			return nil, fmt.Errorf("specified configuration file does not exist: %s", configPath)
		}

		// Use the specified path
		v.SetConfigFile(configPath)

	} else {

		// The default name of our YAML configuration file reflects the plugin name
		v.SetConfigName(pluginName)
		v.SetConfigType("yaml")

		// We search for the configuration file in both our global config directory and the current working directory
		v.AddConfigPath(".")
		v.AddConfigPath("\\etc\\directx-device-plugins")
	}

	// Attempt to parse our YAML configuration file if it exists
	if err := v.ReadInConfig(); err != nil {
		if _, ok := err.(viper.ConfigFileNotFoundError); ok {
			logger.Infow("Configuration file not found, using configuration values from environment variables")
		} else {
			return nil, err
		}
	}

	// Load the parsed configuration values into our struct
	c := &PluginConfig{}
	if err := v.Unmarshal(c); err != nil {
		return nil, err
	}

	// Enforce a minimum value of 1 for multitenancy
	if c.Multitenancy == 0 {
		c.Multitenancy = 1
	}

	// Append our default mounts to any user-supplied values
	c.AdditionalMounts = appendMounts(c.AdditionalMounts, mount.DefaultMounts)
	c.AdditionalMountsWow64 = appendMounts(c.AdditionalMountsWow64, mount.DefaultMountsWow64)

	// Log the parsed configuration values
	logger.Infow("Parsed configuration data", "config", c)

	return c, nil
}
