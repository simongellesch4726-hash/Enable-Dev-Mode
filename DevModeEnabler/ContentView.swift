import SwiftUI
import Darwin

struct ContentView: View {
    @State private var statusMessage = "Checking..."
    @State private var isEnabled = false
    @State private var isWorking = false

    var body: some View {
        VStack(spacing: 30) {
            Text("Developer Mode Tool")
                .font(.largeTitle)
                .fontWeight(.bold)

            Text(statusMessage)
                .font(.title3)
                .foregroundColor(isEnabled ? .green : .orange)
                .multilineTextAlignment(.center)
                .padding()

            if !isEnabled {
                Button(action: enableDevMode) {
                    Label("Enable Developer Mode", systemImage: "gearshape.fill")
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(Color.blue)
                        .foregroundColor(.white)
                        .cornerRadius(12)
                }
                .disabled(isWorking)
            } else {
                HStack {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(.green)
                    Text("Enabled")
                        .font(.headline)
                        .foregroundColor(.green)
                }
            }

            Button("Respring Now") {
                respring()
            }
            .padding()
            .background(Color.gray.opacity(0.2))
            .cornerRadius(10)
            .disabled(!isEnabled || isWorking)
        }
        .padding()
        .onAppear {
            checkStatus()
        }
    }

    private func runHelper(_ args: [String]) -> (output: String, status: Int32) {
        guard let helperPath = Bundle.main.path(forResource: "devmode_helper", ofType: nil) else {
            return ("Helper not found", -1)
        }

        var pid: pid_t = 0
        var pipeFds: [Int32] = [0, 0]
        guard pipe(&pipeFds) == 0 else { return ("", -1) }

        let argsC = ([helperPath] + args).map { strdup($0) } + [nil]
        let argv = argsC.map { $0 as UnsafeMutablePointer<CChar>? }
        defer { argsC.forEach { free($0) } }

        var fileActions = posix_spawn_file_actions_t()
        posix_spawn_file_actions_init(&fileActions)
        posix_spawn_file_actions_adddup2(&fileActions, pipeFds[1], STDOUT_FILENO)
        posix_spawn_file_actions_addclose(&fileActions, pipeFds[0])

        var attrs = posix_spawnattr_t()
        posix_spawnattr_init(&attrs)

        let spawnStatus = posix_spawn(&pid, helperPath, &fileActions, &attrs, argv, nil)

        posix_spawn_file_actions_destroy(&fileActions)
        posix_spawnattr_destroy(&attrs)
        close(pipeFds[1])

        guard spawnStatus == 0 else {
            close(pipeFds[0])
            return ("", spawnStatus)
        }

        var waitStatus: Int32 = 0
        waitpid(pid, &waitStatus, 0)

        var output = ""
        let bufferSize = 1024
        var buffer = [CChar](repeating: 0, count: bufferSize)
        while true {
            let bytesRead = read(pipeFds[0], &buffer, bufferSize - 1)
            if bytesRead <= 0 { break }
            buffer[bytesRead] = 0
            output += String(cString: buffer)
        }
        close(pipeFds[0])

        let exitStatus: Int32
        if (waitStatus & 0x7f) == 0 {
            exitStatus = (waitStatus >> 8) & 0xff
        } else {
            exitStatus = -1
        }

        return (output.trimmingCharacters(in: .whitespacesAndNewlines), exitStatus)
    }

    func checkStatus() {
        let result = runHelper(["status"])
        if result.status == 0 {
            if result.output == "1" {
                isEnabled = true
                statusMessage = "✅ Developer Mode: ENABLED"
            } else {
                isEnabled = false
                statusMessage = "❌ Developer Mode: DISABLED"
            }
        } else {
            statusMessage = "Error: \(result.output)"
        }
    }

    func enableDevMode() {
        isWorking = true
        statusMessage = "Enabling..."
        DispatchQueue.global(qos: .userInitiated).async {
            let result = runHelper(["enable"])
            DispatchQueue.main.async {
                isWorking = false
                if result.status == 0 {
                    isEnabled = true
                    statusMessage = "✅ Developer Mode enabled!"
                } else {
                    statusMessage = "❌ Enable failed: \(result.output)"
                }
            }
        }
    }

    func respring() {
        statusMessage = "Respringing..."
        DispatchQueue.global(qos: .userInitiated).async {
            _ = runHelper(["respring"])
        }
    }
}
