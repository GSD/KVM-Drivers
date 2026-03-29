// MainWindow.xaml.cs - KVM Control Panel Code-Behind
using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;
using System.ComponentModel;

namespace KVM.Tray
{
    public partial class MainWindow : Window
    {
        private ObservableCollection<ConnectionInfo> connections;
        private DispatcherTimer refreshTimer;
        private AppSettings settings;
        
        public MainWindow()
        {
            InitializeComponent();
            LoadSettings();
            InitializeData();
            SetupTimer();
            ApplySettings();
        }

        private void LoadSettings()
        {
            settings = SettingsManager.LoadSettings();
            
            // Check command line for minimized start
            string[] args = Environment.GetCommandLineArgs();
            foreach (string arg in args)
            {
                if (arg.Equals("--minimized", StringComparison.OrdinalIgnoreCase))
                {
                    settings.StartMinimized = true;
                    break;
                }
            }
        }

        private void ApplySettings()
        {
            // Apply network settings
            VncPort.Text = settings.VncPort.ToString();
            WsPort.Text = settings.WebSocketPort.ToString();
            VncRequireAuth.IsChecked = settings.VncRequireAuth;
            UseTls.IsChecked = settings.UseTls;
            
            // Apply driver auto-start if enabled
            if (settings.AutoStartDrivers)
            {
                AutoStartDrivers();
            }
            
            // Minimize if requested
            if (settings.StartMinimized)
            {
                WindowState = WindowState.Minimized;
                Hide();
            }
        }

        private void AutoStartDrivers()
        {
            // Auto-start enabled drivers
            if (settings.KeyboardEnabled) StartDriver("Keyboard", KeyboardStatus, KeyboardState, null);
            if (settings.MouseEnabled) StartDriver("Mouse", MouseStatus, MouseState, null);
            if (settings.ControllerEnabled) StartDriver("Controller", ControllerStatus, ControllerState, null);
            if (settings.DisplayEnabled) StartDriver("Display", DisplayStatus, DisplayState, null);
        }

        private void InitializeData()
        {
            connections = new ObservableCollection<ConnectionInfo>();
            ConnectionsList.ItemsSource = connections;
            
            // Add sample data for now
            connections.Add(new ConnectionInfo 
            { 
                ClientIP = "192.168.1.100", 
                ConnectionType = "VNC", 
                ConnectTime = DateTime.Now.ToString("HH:mm:ss") 
            });
        }

        private void SetupTimer()
        {
            refreshTimer = new DispatcherTimer();
            refreshTimer.Interval = TimeSpan.FromSeconds(2);
            refreshTimer.Tick += RefreshTimer_Tick;
            refreshTimer.Start();
        }

        private void RefreshTimer_Tick(object sender, EventArgs e)
        {
            UpdateDriverStatus();
        }

        private void UpdateDriverStatus()
        {
            // Check actual driver status via service or driver interface
            // For now, just UI refresh
        }

        // Driver Control Events
        private void KeyboardToggle_Click(object sender, RoutedEventArgs e)
        {
            ToggleDriver("Keyboard", KeyboardStatus, KeyboardState, (Button)sender);
        }

        private void MouseToggle_Click(object sender, RoutedEventArgs e)
        {
            ToggleDriver("Mouse", MouseStatus, MouseState, (Button)sender);
        }

        private void ControllerToggle_Click(object sender, RoutedEventArgs e)
        {
            ToggleDriver("Controller", ControllerStatus, ControllerState, (Button)sender);
        }

        private void DisplayToggle_Click(object sender, RoutedEventArgs e)
        {
            ToggleDriver("Display", DisplayStatus, DisplayState, (Button)sender);
        }

        private void ToggleDriver(string driverName, Ellipse statusIndicator, 
            TextBlock stateText, Button toggleButton)
        {
            bool isRunning = stateText.Text == "Running";
            
            if (isRunning)
            {
                StopDriver(driverName, statusIndicator, stateText, toggleButton);
            }
            else
            {
                StartDriver(driverName, statusIndicator, stateText, toggleButton);
            }
            
            // Save driver state to settings
            SaveDriverState(driverName, !isRunning);
        }
        
        private void StartDriver(string driverName, Ellipse statusIndicator, 
            TextBlock stateText, Button toggleButton)
        {
            // Update UI
            if (statusIndicator != null) statusIndicator.Fill = Brushes.Green;
            if (stateText != null) stateText.Text = "Running";
            if (toggleButton != null) toggleButton.Content = "Stop";
            
            // TODO: Actual driver start via service
            AppendLog($"Driver '{driverName}' started");
        }
        
        private void StopDriver(string driverName, Ellipse statusIndicator, 
            TextBlock stateText, Button toggleButton)
        {
            // Update UI
            if (statusIndicator != null) statusIndicator.Fill = Brushes.Gray;
            if (stateText != null) stateText.Text = "Stopped";
            if (toggleButton != null) toggleButton.Content = "Start";
            
            // TODO: Actual driver stop via service
            AppendLog($"Driver '{driverName}' stopped");
        }
        
        private void SaveDriverState(string driverName, bool running)
        {
            switch (driverName)
            {
                case "Keyboard": settings.KeyboardEnabled = running; break;
                case "Mouse": settings.MouseEnabled = running; break;
                case "Controller": settings.ControllerEnabled = running; break;
                case "Display": settings.DisplayEnabled = running; break;
            }
            SettingsManager.SaveSettings(settings);
        }

        // Log Events
        private void ClearLogs_Click(object sender, RoutedEventArgs e)
        {
            LogViewer.Clear();
        }

        private void ExportLogs_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string logPath = System.IO.Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "KVM-Drivers", "kvmlogs.txt");
                
                System.IO.File.WriteAllText(logPath, LogViewer.Text);
                MessageBox.Show($"Logs exported to {logPath}", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to export logs: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        public void AppendLog(string message)
        {
            string timestamp = DateTime.Now.ToString("HH:mm:ss");
            Dispatcher.Invoke(() =>
            {
                LogViewer.AppendText($"[{timestamp}] {message}\r\n");
                LogViewer.ScrollToEnd();
                
                // Limit log buffer
                if (LogViewer.Text.Length > 100000)
                {
                    LogViewer.Text = LogViewer.Text.Substring(LogViewer.Text.Length - 50000);
                }
            });
        }

        // Remote Events
        private void RestartServer_Click(object sender, RoutedEventArgs e)
        {
            // TODO: Actually restart server
            MessageBox.Show("Server restarted", "Restart", MessageBoxButton.OK, MessageBoxImage.Information);
            AppendLog("Remote server restarted");
        }

        private void DisconnectClient_Click(object sender, RoutedEventArgs e)
        {
            var button = sender as Button;
            var connection = button?.DataContext as ConnectionInfo;
            if (connection != null)
            {
                connections.Remove(connection);
                AppendLog($"Disconnected client {connection.ClientIP}");
            }
        }

        // Settings Events
        private void SaveSettings_Click(object sender, RoutedEventArgs e)
        {
            // Update settings from UI
            if (int.TryParse(VncPort.Text, out int vncPort))
                settings.VncPort = vncPort;
            
            if (int.TryParse(WsPort.Text, out int wsPort))
                settings.WebSocketPort = wsPort;
                
            settings.VncRequireAuth = VncRequireAuth.IsChecked ?? true;
            settings.UseTls = UseTls.IsChecked ?? true;
            settings.AutoStartWithWindows = AutoStartCheckBox.IsChecked ?? false;
            settings.MinimizeToTray = MinimizeToTrayCheckBox.IsChecked ?? true;
            
            // Save to disk
            if (SettingsManager.SaveSettings(settings))
            {
                // Set auto-start registry
                SettingsManager.SetAutoStartWithWindows(settings.AutoStartWithWindows);
                
                MessageBox.Show("Settings saved successfully", "Save", MessageBoxButton.OK, MessageBoxImage.Information);
                AppendLog("Settings saved");
            }
            else
            {
                MessageBox.Show("Failed to save settings", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ResetSettings_Click(object sender, RoutedEventArgs e)
        {
            if (MessageBox.Show("Reset all settings to defaults?", "Confirm", 
                MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes)
            {
                SettingsManager.ResetToDefaults();
                settings = SettingsManager.LoadSettings();
                ApplySettings();
                AppendLog("Settings reset to defaults");
            }
        }

        private void ImportSettings_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*",
                Title = "Import Settings"
            };
            
            if (dialog.ShowDialog() == true)
            {
                if (SettingsManager.ImportSettings(dialog.FileName))
                {
                    settings = SettingsManager.LoadSettings();
                    ApplySettings();
                    MessageBox.Show("Settings imported successfully", "Import", MessageBoxButton.OK, MessageBoxImage.Information);
                    AppendLog("Settings imported");
                }
                else
                {
                    MessageBox.Show("Failed to import settings", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void ExportSettings_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new Microsoft.Win32.SaveFileDialog
            {
                Filter = "JSON files (*.json)|*.json|All files (*.*)|*.*",
                Title = "Export Settings",
                FileName = "kvm-settings.json"
            };
            
            if (dialog.ShowDialog() == true)
            {
                if (SettingsManager.ExportSettings(dialog.FileName))
                {
                    MessageBox.Show("Settings exported successfully", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
                    AppendLog("Settings exported");
                }
                else
                {
                    MessageBox.Show("Failed to export settings", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        protected override void OnClosing(CancelEventArgs e)
        {
            refreshTimer?.Stop();
            base.OnClosing(e);
        }
        
        protected override void OnStateChanged(EventArgs e)
        {
            if (WindowState == WindowState.Minimized && settings.MinimizeToTray)
            {
                Hide();
            }
            base.OnStateChanged(e);
        }
    }

    public class ConnectionInfo
    {
        public string ClientIP { get; set; }
        public string ConnectionType { get; set; }
        public string ConnectTime { get; set; }
    }
}
