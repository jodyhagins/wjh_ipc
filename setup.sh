#!/bin/sh

echo "🔧 Setting up Git hooks..."
git config core.hooksPath .githooks
echo "✅ Git hooks are now configured!"
