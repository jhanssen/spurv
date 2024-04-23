export interface ProcessFinishedEvent {
    type: "finished";
    exitCode: number;
    error?: string;
    stdout?: ArrayBuffer;
    stderr?: ArrayBuffer;
}
