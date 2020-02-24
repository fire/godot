import os  # system()

def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "STTConfig",
        "STTQueue",
        "STTRunner",
        "STTError",
    ]


def get_doc_path():
    return "doc"
