// ConnectionApprovalDialog.xaml.cs - Remote connection request approval UI
using System;
using System.Windows;
using System.Windows.Threading;

namespace KVM.Tray
{
    /// <summary>
    /// Dialog for approving or rejecting incoming remote connections
    /// Shows client details, IP, protocol, and allows time-limited access
    /// </summary>
    public partial class ConnectionApprovalDialog : Window
    {
        private DispatcherTimer _countdownTimer;
        private int _remainingSeconds;
        private bool _decisionMade;
        
        public ConnectionResult Result { get; private set; }
        public int ApprovedDurationMinutes { get; private set; }
        
        public class ConnectionRequest
        {
            public string ClientIP { get; set; }
            public string Hostname { get; set; }
            public string Protocol { get; set; }  // VNC, WebSocket, etc.
            public string RequestedAccess { get; set; }  // View, Control, etc.
            public DateTime RequestTime { get; set; }
            public string GeoLocation { get; set; }
            public bool IsAuthenticated { get; set; }
        }
        
        public enum ConnectionResult
        {
            Approved,
            Rejected,
            TimedOut,
            BlockedPermanently
        }
        
        public ConnectionApprovalDialog(ConnectionRequest request)
        {
            InitializeComponent();
            DataContext = request;
            
            _remainingSeconds = 30; // 30 second timeout
            ApprovedDurationMinutes = 60; // Default 1 hour
            
            SetupUI(request);
            StartCountdown();
        }
        
        private void SetupUI(ConnectionRequest request)
        {
            // Set window title based on protocol
            Title = $"Connection Request - {request.Protocol}";
            
            // Update labels
            ClientIPText.Text = request.ClientIP;
            HostnameText.Text = string.IsNullOrEmpty(request.Hostname) ? "Unknown" : request.Hostname;
            ProtocolText.Text = request.Protocol;
            AccessTypeText.Text = request.RequestedAccess;
            RequestTimeText.Text = request.RequestTime.ToString("HH:mm:ss");
            
            // Show authentication status
            if (request.IsAuthenticated)
            {
                AuthStatusText.Text = "✓ Authenticated";
                AuthStatusText.Foreground = System.Windows.Media.Brushes.Green;
            }
            else
            {
                AuthStatusText.Text = "✗ Not Authenticated";
                AuthStatusText.Foreground = System.Windows.Media.Brushes.Orange;
            }
            
            // Geo location if available
            if (!string.IsNullOrEmpty(request.GeoLocation))
            {
                GeoLocationText.Text = request.GeoLocation;
                GeoLocationPanel.Visibility = Visibility.Visible;
            }
            else
            {
                GeoLocationPanel.Visibility = Visibility.Collapsed;
            }
            
            // Set up duration combo box
            DurationComboBox.ItemsSource = new[] { 15, 30, 60, 120, 240, 480 };
            DurationComboBox.SelectedItem = 60;
        }
        
        private void StartCountdown()
        {
            CountdownText.Text = $"Auto-reject in {_remainingSeconds}s";
            
            _countdownTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(1)
            };
            
            _countdownTimer.Tick += (s, e) =>
            {
                _remainingSeconds--;
                CountdownText.Text = $"Auto-reject in {_remainingSeconds}s";
                
                if (_remainingSeconds <= 0 && !_decisionMade)
                {
                    _decisionMade = true;
                    Result = ConnectionResult.TimedOut;
                    _countdownTimer.Stop();
                    DialogResult = false;
                    Close();
                }
            };
            
            _countdownTimer.Start();
        }
        
        private void ApproveButton_Click(object sender, RoutedEventArgs e)
        {
            if (_decisionMade) return;
            
            _decisionMade = true;
            _countdownTimer?.Stop();
            
            Result = ConnectionResult.Approved;
            ApprovedDurationMinutes = (int)DurationComboBox.SelectedItem;
            
            DialogResult = true;
            Close();
        }
        
        private void RejectButton_Click(object sender, RoutedEventArgs e)
        {
            if (_decisionMade) return;
            
            _decisionMade = true;
            _countdownTimer?.Stop();
            
            Result = ConnectionResult.Rejected;
            
            DialogResult = false;
            Close();
        }
        
        private void BlockPermanentlyButton_Click(object sender, RoutedEventArgs e)
        {
            if (_decisionMade) return;
            
            var result = MessageBox.Show(
                $"Are you sure you want to permanently block {((ConnectionRequest)DataContext).ClientIP}?\n\n" +
                "This will add the IP to your block list.",
                "Confirm Permanent Block",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);
            
            if (result == MessageBoxResult.Yes)
            {
                _decisionMade = true;
                _countdownTimer?.Stop();
                
                Result = ConnectionResult.BlockedPermanently;
                
                DialogResult = false;
                Close();
            }
        }
        
        protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
        {
            _countdownTimer?.Stop();
            base.OnClosing(e);
        }
    }
}
