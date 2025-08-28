# Contributing to Grid Board Tab5

First off, thank you for considering contributing to Grid Board Tab5! It's people like you that make this project better for everyone.

## Code of Conduct

This project and everyone participating in it is governed by our Code of Conduct. By participating, you are expected to uphold this code.

## How Can I Contribute?

### Reporting Bugs

Before creating bug reports, please check existing issues to avoid duplicates. When you create a bug report, include as many details as possible:

- **Use a clear and descriptive title**
- **Describe the exact steps to reproduce the problem**
- **Provide specific examples**
- **Describe the behavior you observed and expected**
- **Include logs and screenshots if possible**
- **Include your environment details** (ESP-IDF version, OS, hardware version)

### Suggesting Enhancements

Enhancement suggestions are tracked as GitHub issues. When creating an enhancement suggestion:

- **Use a clear and descriptive title**
- **Provide a step-by-step description of the suggested enhancement**
- **Provide specific examples to demonstrate the steps**
- **Describe the current behavior and expected behavior**
- **Explain why this enhancement would be useful**

### Pull Requests

1. Fork the repo and create your branch from `main`
2. If you've added code that should be tested, add tests
3. Ensure the test suite passes
4. Make sure your code follows the existing code style
5. Issue that pull request!

## Development Process

1. **Setup Development Environment**
   ```bash
   git clone https://github.com/yourusername/grid-board-tab5.git
   cd grid-board-tab5
   git checkout -b feature/your-feature-name
   ```

2. **Make Your Changes**
   - Write clean, commented code
   - Follow the existing code style
   - Update documentation as needed
   - Add tests if applicable

3. **Test Your Changes**
   ```bash
   idf.py build
   idf.py -p /dev/cu.usbmodem212301 flash monitor
   ```

4. **Commit Your Changes**
   ```bash
   git add .
   git commit -m "Add feature: description of your changes"
   ```

5. **Push to GitHub**
   ```bash
   git push origin feature/your-feature-name
   ```

## Code Style Guidelines

### C++ Code Style

- Use 4 spaces for indentation (no tabs)
- Place opening braces on the same line
- Use descriptive variable names
- Add comments for complex logic
- Keep functions small and focused

Example:
```cpp
void processMessage(const char* message) {
    // Process each character in the message
    for (int i = 0; message[i] != '\0'; i++) {
        // Handle special characters
        if (isSpecialChar(message[i])) {
            handleSpecialChar(message[i]);
        }
    }
}
```

### File Organization

- Keep related functionality in the same file
- Use clear, descriptive filenames
- Organize includes: system headers, ESP-IDF headers, project headers
- Use header guards in all .h files

### Documentation

- Document all public functions
- Use clear, concise comments
- Update README.md for user-facing changes
- Include examples where helpful

## Testing

Before submitting a PR:

1. **Build Test**: Ensure the project builds without warnings
2. **Flash Test**: Verify it flashes successfully to hardware
3. **Functional Test**: Test all affected features
4. **Edge Cases**: Test boundary conditions

## Git Commit Messages

- Use the present tense ("Add feature" not "Added feature")
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...")
- Limit the first line to 72 characters or less
- Reference issues and pull requests liberally after the first line

Examples:
```
Add emoji rendering support for grid cells

- Implement Unicode character detection
- Add emoji-to-grid mapping logic
- Update display refresh for emoji rendering
Fixes #123
```

## Project Structure

When adding new features, maintain the existing project structure:

```
main/           - Main application code
components/     - Reusable components
docs/          - Documentation files
scripts/       - Build and utility scripts
```

## Questions?

Feel free to open an issue with your question or reach out to the maintainers.

## Recognition

Contributors will be recognized in the project README and release notes.

Thank you for contributing!