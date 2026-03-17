#[derive(Debug, Default, Clone)]
pub struct SessionState {
    attached_files: Vec<String>,
    pub last_task: Option<String>,
    pub completed_runs: usize,
}

impl SessionState {
    pub fn new(attached_files: Vec<String>) -> Self {
        Self {
            attached_files,
            last_task: None,
            completed_runs: 0,
        }
    }

    pub fn attached_files(&self) -> &[String] {
        &self.attached_files
    }

    pub fn add_attached_file(&mut self, path: String) {
        self.attached_files.push(path);
    }

    pub fn clear_attached_files(&mut self) {
        self.attached_files.clear();
    }

    pub fn mark_completed_run(&mut self, task: &str) {
        self.last_task = Some(task.to_string());
        self.completed_runs += 1;
    }
}
