// App.xaml.cs - Tray Application Entry Point
using System.Windows;
using Hardcodet.Wpf.TaskbarNotification;

namespace KVM.Tray
{
    public partial class App : Application
    {
        private TaskbarIcon? _trayIcon;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            _trayIcon = (TaskbarIcon)FindResource("TrayIcon");
        }

        protected override void OnExit(ExitEventArgs e)
        {
            _trayIcon?.Dispose();
            base.OnExit(e);
        }

        private void TrayShowPanel_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            var win = MainWindow;
            if (win != null)
            {
                win.Show();
                win.WindowState = WindowState.Normal;
                win.Activate();
            }
        }

        private void TrayExit_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            Shutdown();
        }
    }
}
