
# 🛠 Contributing to the Project

Thank you for your interest in contributing!
To maintain a high-quality and consistent codebase, we have a set of guidelines
and automated checks in place.

---

## ✅ **Before You Start**
1. **Clone the repository**:
   ```bash
   git clone https://github.com/jodyhagins/wjh_ipc.git
   cd wjh_ipc
   ```

2. **Run the one-time setup script to install Git hooks**:
   ```bash
   sh setup-hooks.sh
   ```
   - This ensures that all required setup steps are performed, including a
     pre-commit hook.

---

## 🔹 **Rules and Expectations**
### **❌ What is NOT allowed?**
- 🚫 **Direct commits to `main`**
- 🚫 **Bypassing pre-commit hooks** (All code must be formatted before pushing).
- 🚫 **Committing via GitHub Web UI**

### **✅ What is Expected?**
- ✅ **All changes must go through a Pull Request (PR).**
- ✅ **Pre-commit hooks must be run before pushing.**
- ✅ **Code must follow the project's formatting and linting rules.**
- ✅ **All PRs must be reviewed and approved before merging.**

---

## 🔄 **Pre-commit Hook and Formatting**
This project enforces **automatic source code formatting** using the pre-commit hook.  
Before pushing any code, ensure that all formatting is correct.

### **Building the Formatter**
The code formatter needs to be available before it can be used.
It is a special fork of clang-format.
Because.

There are multiple ways to perform formatting (see the pre-commit script for
details), but the easiest way is:

1. Enable the formatter in CMake when configuring the build:
   ```bash
   cmake -S <source-dir> -B <build-dir> -DWJH_IPC_BUILD_WJH_FORMAT=On
   ```

2. Run the formatter using CMake:
   ```bash
   cmake --build <build-dir> --target run-format
   ```
This will format all files that are staged for commit.

The pre-commit hook will verify that formatting has been performed.
Note that the pre-commit hook runs verification only.
The pre-commit hook does not modify any files.
There are likely ways to get around pushing only formatted code.
Don't do it.

---

## 🔀 **Creating a Pull Request**
1. **Create a feature branch**:
   ```bash
   git checkout -b my-feature-branch
   ```

2. **Make your changes and commit**:
   ```bash
   git add .
   git commit -m "Add feature X"
   ```

3. **Push your branch to GitHub**:
   ```bash
   git push origin my-feature-branch
   ```

4. **Create a Pull Request (PR)**
   - Go to the repository on GitHub.
   - Click **"New Pull Request"**.
   - Select your feature branch and submit the PR for review.

---

## 📌 **Pull Request Guidelines**
- Your PR **must pass all automated checks** before merging.
- The **pre-commit hook must run successfully**
- Follow clear and descriptive commit messages.
- Ensure your PR **does not introduce unnecessary changes**.

---

## ❗ **Need Help?**
If you have any questions, feel free to **open an issue** or reach out to the
maintainers.

---

🚀 **Happy coding! We appreciate your contributions!** 🚀
