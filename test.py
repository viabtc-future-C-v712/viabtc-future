def read_increment_variable(file_path):
    try:
        with open(file_path, 'r') as file:
            content = file.read()
            return int(content)
    except FileNotFoundError:
        return 1  # 如果文件不存在，返回初始值

def write_increment_variable(file_path, value):
    with open(file_path, 'w') as file:
        file.write(str(value))
