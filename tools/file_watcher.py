#!/usr/bin/env python3
"""
文件监控服务
监控 shared/ 目录下的 JSON 文件变化，自动触发代码生成
"""

import logging
import os
import sys
import time
import threading
from pathlib import Path
from typing import Set

# 配置日志（在导入生成脚本之前，确保日志目录存在）
log_dir = Path(__file__).parent.parent / 'logs'
log_dir.mkdir(exist_ok=True)

log_file = log_dir / 'file_watcher.log'
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_file, encoding='utf-8'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler, FileModifiedEvent, FileCreatedEvent, FileDeletedEvent

# 导入生成脚本
sys.path.insert(0, str(Path(__file__).parent))
from generate_constants import process_config_file


class ConfigFileHandler(FileSystemEventHandler):
    """配置文件变化处理器"""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.shared_dir = project_root / 'shared'
        self.pending_files: Set[Path] = set()
        self.debounce_timer: threading.Timer | None = None
        self.debounce_delay = 0.5  # 500ms 防抖延迟

    def _is_config_file(self, path: str) -> bool:
        """检查是否是配置文件"""
        return path.endswith('.json') and 'shared' in path

    def _debounce_callback(self) -> None:
        """防抖回调：处理所有待处理的文件"""
        if not self.pending_files:
            return

        logger.info("检测到文件变化，开始重新生成...")

        success_count = 0
        fail_count = 0

        for json_file in sorted(self.pending_files):
            if json_file.exists():  # 文件删除时不处理
                if process_config_file(json_file, self.project_root):
                    success_count += 1
                else:
                    fail_count += 1

        self.pending_files.clear()
        logger.info(f"重新生成完成: 成功 {success_count}, 失败 {fail_count}")

    def _schedule_generation(self, path: str) -> None:
        """调度生成任务（带防抖）"""
        if not self._is_config_file(path):
            return

        json_file = Path(path)
        self.pending_files.add(json_file)

        # 取消之前的定时器
        if self.debounce_timer is not None:
            self.debounce_timer.cancel()

        # 创建新的定时器
        self.debounce_timer = threading.Timer(self.debounce_delay, self._debounce_callback)
        self.debounce_timer.daemon = True
        self.debounce_timer.start()

    def on_modified(self, event: FileModifiedEvent) -> None:
        """文件修改事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)

    def on_created(self, event: FileCreatedEvent) -> None:
        """文件创建事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)

    def on_deleted(self, event: FileDeletedEvent) -> None:
        """文件删除事件"""
        if not event.is_directory:
            self._schedule_generation(event.src_path)


def main() -> int:
    """主函数"""
    # 获取项目根目录
    project_root = Path(__file__).parent.parent
    shared_dir = project_root / 'shared'

    # 检查 shared 目录是否存在
    if not shared_dir.exists():
        logger.error(f"shared 目录不存在: {shared_dir}")
        return 1

    logger.info("启动文件监控服务...")
    logger.info(f"监控目录: {shared_dir}")
    logger.info("按 Ctrl+C 停止服务")

    # 创建事件处理器
    event_handler = ConfigFileHandler(project_root)

    # 创建观察者
    observer = Observer()
    observer.schedule(event_handler, str(shared_dir), recursive=False)

    # 启动观察者
    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logger.info("停止文件监控服务...")
        observer.stop()

    observer.join()
    logger.info("文件监控服务已停止")

    return 0


if __name__ == '__main__':
    sys.exit(main())
