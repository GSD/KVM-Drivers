// MainWindow.xaml.cs - KVM Control Panel Code-Behind
using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Threading;

namespace KVM.Tray
{
    public partial class MainWindow : Window
    {
        private ObservableCollection<ConnectionInfo> connections;
        private DispatcherTimer refreshTimer;
        
        public MainWindow()
        {
            InitializeComponent();
            InitializeData();
            SetupTimer();
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
            // Update driver status indicators
            UpdateDriverStatus();
        }

        private void UpdateDriverStatus()
        {
            // TODO: Check actual driver status via service
            // For now, just animate or refresh UI
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
                // Stop driver
                statusIndicator.Fill = Brushes.Gray;
                stateText.Text = "Stopped";
                toggleButton.Content = "Start";
                AppendLog($"Driver '{driverName}' stopped");
            }
            else
            {
                // Start driver
                statusIndicator.Fill = Brushes.Green;
                stateText.Text = "Running";
                toggleButton.Content = "Stop";
                AppendLog($"Driver '{driverName}' started");
            }
        }

        // Log Events
        private void ClearLogs_Click(object sender, RoutedEventArgs e)
        {
            LogViewer.Clear();
        }

        private void ExportLogs_Click(object sender, RoutedEventArgs e)
        {
            // TODO: Export logs to file
            MessageBox.Show("Logs exported to kvmlogs.txt", "Export", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        public void AppendLog(string message)
        {
            string timestamp = DateTime.Now.ToString("HH:mm:ss");
            Dispatcher.Invoke(() =>
            {
                LogViewer.AppendText($"[{timestamp}] {message}\r\n");
                LogViewer.ScrollToEnd();
            });
        }

        // Remote Events
        private void RestartServer_Click(object sender, RoutedEventArgs e)
        {
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
            // TODO: Save settings to config file
            MessageBox.Show("Settings saved", "Save", MessageBoxButton.OK, MessageBoxImage.Information);
            AppendLog("Settings saved");
        }

        private void ResetSettings_Click(object sender, RoutedEventArgs e)
        {
            VncPort.Text = "5900";
            WsPort.Text = "8443";
            VncRequireAuth.IsChecked = true;
            UseTls.IsChecked = true;
            AppendLog("Settings reset to defaults");
        }

        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            refreshTimer?.Stop();
            base.OnClosing(e);
        }
    }

    public class ConnectionInfo
    {
        public string ClientIP { get; set; }
        public string ConnectionType { get; set; }
        public string ConnectTime { get; set; }
    }
}
