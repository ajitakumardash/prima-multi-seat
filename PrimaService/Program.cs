// ============================================================
// Prima Multi Seat - Windows Service Entry Point
// Hosts PrimaWorker as a Windows Service with auto-restart
// ============================================================

using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using PrimaService;

var host = Host.CreateDefaultBuilder(args)
    .UseWindowsService(options => {
        options.ServiceName = "PrimaMultiSeatService";
    })
    .ConfigureLogging(logging => {
        logging.AddEventLog(settings => {
            settings.SourceName = "PrimaMultiSeat";
        });
        logging.AddConsole();
    })
    .ConfigureServices(services => {
        services.AddHostedService<PrimaWorker>();
    })
    .Build();

await host.RunAsync();
